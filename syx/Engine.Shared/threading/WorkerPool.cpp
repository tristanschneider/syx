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

void WorkerPool::queueTask(std::shared_ptr<Task> task) {
  assert(!task->hasWorkerPool() && "Task should only be queued once");
  if(!task->hasWorkerPool()) {
    std::lock_guard<std::mutex> lock(mTaskMutex);
    task->setWorkerPool(*this);
    if(!task->hasDependencies()) {
      _taskReady(task);
    }
  }
}

void WorkerPool::taskReady(std::shared_ptr<Task> task) {
  std::lock_guard<std::mutex> lock(mTaskMutex);
  //If it has a pool that means queueTask has been called on it, and all of its dependencies have now completed, so it can run
  //Otherwise, all dependencies are complete, but it hasn't been queued yet, so the user may still add more dependencies before queueing
  if(task->hasWorkerPool()) {
    _taskReady(task);
  }
}

void WorkerPool::_taskReady(std::shared_ptr<Task> task) {
  //Tasks can be queued on queueTask or when its dependencies are done
  //Make sure not to add the task twice
  if(!task->hasBeenQueued()) {
    task->setQueued();
    mTasks.push_back(std::move(task));
    mWorkerCV.notify_one();
  }
}

void WorkerPool::_workerLoop() {
  while(!mTerminate) {
    //Use same lock for condition variable as tasks, so if work is being queued,
    //a thread doesn't miss the notification then immediately wait on the condition variable
    std::unique_lock<std::mutex> taskLock(mTaskMutex);
    if(std::shared_ptr<Task> task = _getTask()) {
      taskLock.unlock();
      task->run();
    }
    //No work, but termination might have been signaled as we were grabbing mutex, check before sleeping
    else if(!mTerminate) {
      //Now work now, wait until some is available
      mWorkerCV.wait(taskLock);
    }
  }
}

std::shared_ptr<Task> WorkerPool::_getTask() {
  std::shared_ptr<Task> result = nullptr;
  //Order doesn't matter as they're all tasks that have no dependencies left, so grab the last one
  if(mTasks.size()) {
    result = std::move(mTasks.back());
    mTasks.pop_back();
  }
  return result;
}