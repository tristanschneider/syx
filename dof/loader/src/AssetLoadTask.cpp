#include "Precompile.h"
#include "AssetLoadTask.h"

#include "AppBuilder.h"

namespace Loader {
  AssetLoadTask::AssetLoadTask(AssetLoadTaskArgs&& args)
    : taskArgs{ std::move(args) }
  {
  }

  //True when the entire linked list of tasks has completed
  //New elements are only added while tasks are in progress so false positives aren't possible.
  bool AssetLoadTask::isDone() const {
    return task->isDone() && (!next || next->isDone());
  }

  //True if this AssetHandle is pointing at a table somewhere vs being a pending handle
  bool AssetLoadTask::hasStorage() const {
    return !taskArgs.hasPendingHandle;
  }

  //Start a new subtask from the current task that is added to a linked list of tasks needed for completion of the overall asset
  void AssetLoadTask::addTask(AppTaskArgs& args, TaskCallback&& subtask) {
    addTask(next, createPendingHandle(taskArgs.deps.mappings), args, taskArgs.deps, std::move(subtask));
  }

  void AssetLoadTask::addTask(std::shared_ptr<AssetLoadTask>& head, const AssetHandle& self, AppTaskArgs& args, const AssetLoadTaskDeps& deps, TaskCallback&& subtask) {
    assert(!head || head->task->isDone() && "Modification can only happen during the owning task");
    auto child = std::make_shared<AssetLoadTask>(AssetLoadTaskArgs{
      .self = self,
      .deps = deps
    });
    //Add to linked list. Order doesn't matter
    //Do this before queueing the task because the task may further modify its node in the list while in progress
    child->next = head;
    head = child;

    child->task = args.scheduler->queueLongTask([child, t{ std::move(subtask) }](AppTaskArgs& args) {
      t(args, *child);
    }, {});
  }

  //Creates an asset handle with a new reserved ElementRef. It won't point anywhere until updateRequestProgress moves it to a table,
  //but in the mean time it can still be used for assets to refer to each other, like a mesh on what texture it expects
  AssetHandle AssetLoadTask::createPendingHandle(StableElementMappings& mappings) {
    return AssetHandle::createPending(ElementRef{ mappings.createKey() });
  }
}