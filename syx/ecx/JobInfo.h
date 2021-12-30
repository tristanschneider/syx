#pragma once

#include<optional>

namespace ecx {
  template<class EntityT>
  struct ISystem;

  template<class EntityT>
  struct JobInfo {
    void addDependent(std::shared_ptr<JobInfo<EntityT>> dependent) {
      dependent->mTotalDependencies++;
      mDependents.push_back(std::move(dependent));
    }

    void addDependentNoDuplicate(std::shared_ptr<JobInfo<EntityT>> dependent) {
      if(std::find(mDependents.begin(), mDependents.end(), dependent) == mDependents.end()) {
        addDependent(dependent);
      }
    }

    std::shared_ptr<ISystem<EntityT>> mSystem;
    //Current number of jobs that need to finish before this can start
    std::atomic_uint32_t mDependencies;
    //Total number of jobs that need to finish before this can start, used for resetting
    uint32_t mTotalDependencies = 0;
    //Jobs that are waiting for this to complete
    std::vector<std::shared_ptr<JobInfo>> mDependents;
    std::optional<size_t> mThreadRequirement;
  };
}