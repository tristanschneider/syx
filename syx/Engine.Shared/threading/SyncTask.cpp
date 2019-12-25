#include "Precompile.h"
#include "threading/SyncTask.h"

SyncTask::SyncTask()
  : mDependenciesDone(false) {
}

void SyncTask::sync() {
  std::unique_lock<std::mutex> l(mMutex);
  //If dependencies are already done there's nothing to wait for
  if(mDependenciesDone)
    return;
  //Wait until _run notifies us that dependencies are complete
  mCV.wait(l);
}

void SyncTask::_run() {
  std::unique_lock<std::mutex> l(mMutex);
  //Need bool in addition to cv in case task completes while acquiring mutex
  mDependenciesDone = true;
  mCV.notify_all();
}
