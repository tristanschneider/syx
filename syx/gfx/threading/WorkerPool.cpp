#include "Precompile.h"
#include "threading/WorkerPool.h"
#include "threading/Task.h"

WorkerPool::WorkerPool(size_t workerCount)
  : mTerminate(false)
  , mWorkers(nullptr)
  , mWorkerCount(workerCount) {

  if(mWorkerCount) {
    mWorkers = new std::thread[workerCount];
    for(size_t i = 0; i < workerCount; ++i)
      mWorkers[i] = std::thread(&WorkerPool::_workerLoop, this);
  }
}

WorkerPool::~WorkerPool() {
  mTerminate = true;

  //Need to have mutex when notifying so thread doesn't go to sleep right after notification
  mTaskMutex.lock();
  mWorkerCV.notify_all();
  mTaskMutex.unlock();

  for(size_t i = 0; i < mWorkerCount; ++i)
    mWorkers[i].join();

  delete [] mWorkers;
  mWorkers = nullptr;
}

void WorkerPool::queueTask(std::unique_ptr<Task> task) {
  bool canStartNow = task->canRun();
  mTaskMutex.lock();
  mTasks.push_back(std::move(task));
  if(canStartNow)
    mWorkerCV.notify_one();
  mTaskMutex.unlock();
}

void WorkerPool::sync(std::weak_ptr<TaskGroup> group) {
  bool wakeWorkers = false;
  //Normally there shouldn't be termination during sync because the syncing thread is probably the one who would tear down
  while(!group.expired() && !mTerminate) {
    std::unique_lock<std::mutex> taskLock(mTaskMutex);
    //The work we're waiting on might have finished while we were grabbing mutex. If so, exit
    if(group.expired()) {
      //Don't forgot to wake other workers in case this thread finished a work group and that's what it was syncing on
      if(wakeWorkers)
        mWorkerCV.notify_all();
      break;
    }

    _doNextTask(taskLock, wakeWorkers);
  }
}

void WorkerPool::_workerLoop() {
  bool wakeWorkers = false;
  while(!mTerminate) {
    //Use same lock for condition variable as tasks, so if work is being queued,
    //a thread doesn't miss the notification then immediately wait on the condition variable
    std::unique_lock<std::mutex> taskLock(mTaskMutex);
    _doNextTask(taskLock, wakeWorkers);
  }
}

void WorkerPool::_doNextTask(std::unique_lock<std::mutex>& taskLock, bool& wakeWorkers) {
  std::unique_ptr<Task> task = _getTask();
  //Wake others after grabbing the task, as we don't want completion of the last task to cause other workers to steal from this
  if(wakeWorkers)
    mWorkerCV.notify_all();

  if(task) {
    taskLock.unlock();
    task->run();

    //If completing this task unblocks other tasks, wake up other workers.
    //Ideally you only wake others if more than one task is available, but shared_ptr can't know
    wakeWorkers = task->isLastInGroup();
    task = nullptr;
  }
  else {
    //Termination might have been signaled as we were grabbing mutex, check before sleeping
    if(!mTerminate) {
      //Now work now, wait until some is available
      mWorkerCV.wait(taskLock);
    }
  }
}

std::unique_ptr<Task> WorkerPool::_getTask() {
  std::unique_ptr<Task> result = nullptr;
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
