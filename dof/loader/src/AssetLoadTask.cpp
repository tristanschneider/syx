#include "Precompile.h"
#include "AssetLoadTask.h"

#include "AppBuilder.h"

namespace Loader {
  AssetLoadTask::AssetLoadTask(AssetLoadTaskArgs&& args)
    : taskArgs{ std::move(args) }
  {
  }

  AssetLoadTask::~AssetLoadTask() {
    if(!hasStorage()) {
      taskArgs.deps.mappings.eraseKey(taskArgs.self.asset.getMapping());
      setHandleClaimed();
    }
  }

  const AssetHandle& AssetLoadTask::getAssetHandle() const {
    return taskArgs.self;
  }

  //True when the entire linked list of tasks has completed
  //New elements are only added while tasks are in progress so false positives aren't possible.
  bool AssetLoadTask::isDone() const {
    return task->isDone() && (!next || next->isDone());
  }

  //This is a bit weird since the asset might represent a chain but this is only returning one
  //Right now it's only used to await completion of all tasks in the chain by the caller following
  //the linked list, so it doesn't matter that a single getHandle call doesn't represent the entire chain
  const enki::ICompletable* AssetLoadTask::getHandle() const {
    return task->getHandle();
  }

  //True if this AssetHandle is pointing at a table somewhere vs being a pending handle
  bool AssetLoadTask::hasStorage() const {
    return !taskArgs.hasPendingHandle;
  }

  //Start a new subtask from the current task that is added to a linked list of tasks needed for completion of the overall asset
  std::shared_ptr<AssetLoadTask> AssetLoadTask::addTask(const AppTaskArgs& args, TaskCallback&& subtask) {
    return addTask(next, args, AssetLoadTaskArgs{
      .self = createPendingHandle(taskArgs.deps.mappings),
      .deps = taskArgs.deps,
      .hasPendingHandle = true
    }, std::move(subtask));
  }

  std::shared_ptr<AssetLoadTask> AssetLoadTask::addTask(std::shared_ptr<AssetLoadTask>& head, const AppTaskArgs& args, const AssetLoadTaskArgs& deps, TaskCallback&& subtask) {
    auto child = std::make_shared<AssetLoadTask>(AssetLoadTaskArgs{ deps });
    //Add to linked list. Order doesn't matter
    //Do this before queueing the task because the task may further modify its node in the list while in progress
    child->next = head;
    head = child;

    child->task = args.scheduler->queueLongTask([child, t{ std::move(subtask) }](AppTaskArgs& args) {
      t(args, *child);
    }, {});
    return child;
  }

  //Creates an asset handle with a new reserved ElementRef. It won't point anywhere until updateRequestProgress moves it to a table,
  //but in the mean time it can still be used for assets to refer to each other, like a mesh on what texture it expects
  AssetHandle AssetLoadTask::createPendingHandle(StableElementMappings& mappings) {
    return AssetHandle::createPending(ElementRef{ mappings.createKey() });
  }

  void AssetLoadTask::setHandleClaimed() {
    taskArgs.hasPendingHandle = false;
  }
}