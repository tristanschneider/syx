#pragma once
#include "threading/Task.h"

//Task used to block the current thread until the task's dependencies are complete
class SyncTask : public Task {
public:
  SyncTask();

  //Block this thread until dependencies are complete
  void sync();

protected:
  //Mark dependencies as completed, waking up sleeping thread if it's waiting
  virtual void _run() override;

private:
  std::condition_variable mCV;
  std::mutex mMutex;
  bool mDependenciesDone;
};