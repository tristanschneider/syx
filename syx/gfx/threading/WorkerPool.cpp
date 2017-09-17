#include "Precompile.h"
#include "threading/WorkerPool.h"
#include "threading/Task.h"

WorkerPool::WorkerPool(size_t threadCount)
  : mTerminate(false)
  , mWorkerCount(threadCount) {

  mWorkers = new std::thread[threadCount];
  for(size_t i = 0; i < threadCount; ++i)
    mWorkers[i] = std::thread(&WorkerPool::_workerLoop, this);
}

WorkerPool::~WorkerPool() {
  mTerminate = true;
  _wakeAllWorkers();
  for(size_t i = 0; i < mWorkerCount; ++i)
    mWorkers[i].join();

  delete [] mWorkers;
  mWorkers = nullptr;
}

void WorkerPool::queueTask(std::unique_ptr<Task> task) {
  bool canStartNow = task->canRun();
  mTaskMutex.lock();
  mTasks.push_back(std::move(task));
  mTaskMutex.unlock();
  if(canStartNow)
    _wakeOneWorker();
}

void WorkerPool::_workerLoop() {
  bool wakeWorkers = false;
  std::unique_ptr<Task> task = nullptr;
  while(!mTerminate) {
    if(!task)
      task = _getTask();

    if(task) {
      //Wake others after grabbing the task, as we don't want completion of the last task to cause other workers to steal from this
      if(wakeWorkers)
        _wakeAllWorkers();

      task->run();

      //If completing this task unblocks other tasks, wake up other workers.
      //Ideally you only wake others if more than one task is available, but shared_ptr can't know
      wakeWorkers = task->isLastInGroup();
      task = nullptr;
    }
    else {
      //Now work now, wait until some is available
      std::unique_lock<std::mutex> waitLock(mWorkerCVMutex);

      //Work might have become available while we were grabbing the lock, if so, don't sleep now, do work
      if(task = _getTask())
        continue;
      if(mTerminate)
        break;

      mWorkerCV.wait(waitLock);
    }
  }
}

std::unique_ptr<Task> WorkerPool::_getTask() {
  std::unique_ptr<Task> result = nullptr;
  std::lock_guard<std::mutex> lock(mTaskMutex);
  for(size_t i = 0; i < mTasks.size(); ++i) {
    std::unique_ptr<Task>& task = mTasks[i];
    if(task->canRun()) {
      result = std::move(task);
      //No swap remove since we want to preserver order of tasks
      mTasks.erase(mTasks.begin() + i);
      return std::move(result);
    }
  }
  return nullptr;
}

void WorkerPool::_wakeOneWorker() {
  //To make sure we don't try to wake threads right before they go to sleep, lock during notification
  mWorkerCVMutex.lock();
  mWorkerCV.notify_one();
  mWorkerCVMutex.unlock();
}

void WorkerPool::_wakeAllWorkers() {
  mWorkerCVMutex.lock();
  mWorkerCV.notify_all();
  mWorkerCVMutex.unlock();
}
