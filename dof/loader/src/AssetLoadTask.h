#pragma once

#include "AssetVariant.h"
#include "loader/AssetHandle.h"
#include "ILocalScheduler.h"

struct AppTaskArgs;

namespace Loader {
  class AssetIndex;

  struct AssetLoadTaskDeps {
    StableElementMappings& mappings;
    const AssetIndex& index;
  };
  struct AssetLoadTaskArgs {
    AssetHandle self;
    AssetLoadTaskDeps deps;
    //True if `self` is referring to a pending element (is a subtask) or a real one (original task with a table element)
    bool hasPendingHandle{};
  };

  //Linked list of tasks that is modified by the contained ILongTask and read by updateRequestProgress
  //Progress only cares if they are done, which will only look at tasks that have completed
  //Since they only change while incomplete, this means reading is thread safe
  //Modification is then also only ever done by the owning task meaning it is thread safe as well
  struct AssetLoadTask : Tasks::ILongTask {
    using TaskCallback = std::function<void(AppTaskArgs&, AssetLoadTask&)>;

    AssetLoadTask(AssetLoadTaskArgs&& args);
    ~AssetLoadTask();

    const AssetHandle& getAssetHandle() const;

    //True when the entire linked list of tasks has completed
    //New elements are only added while tasks are in progress so false positives aren't possible.
    bool isDone() const final;
    const enki::ICompletable* getHandle() const final;
    //True if this AssetHandle is pointing at a table somewhere vs being a pending handle
    bool hasStorage() const;
    //Start a new subtask from the current task that is added to a linked list of tasks needed for completion of the overall asset
    std::shared_ptr<AssetLoadTask> addTask(const AppTaskArgs& args, TaskCallback&& subtask);
    static std::shared_ptr<AssetLoadTask> addTask(std::shared_ptr<AssetLoadTask>& head, const AppTaskArgs& args, const AssetLoadTaskArgs& deps, TaskCallback&& subtask);
    //Creates an asset handle with a new reserved ElementRef. It won't point anywhere until updateRequestProgress moves it to a table,
    //but in the mean time it can still be used for assets to refer to each other, like a mesh on what texture it expects
    static AssetHandle createPendingHandle(StableElementMappings& mappings);
    //Tasks will release their pending handles if they are not set as claimed before destruction
    //This allows speculatively creating handles that end up going unused
    void setHandleClaimed();

    AssetLoadTaskArgs taskArgs;
    AssetVariant asset;
    std::shared_ptr<Tasks::ILongTask> task;
    std::shared_ptr<AssetLoadTask> next;
  };
}