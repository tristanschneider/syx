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
  task->setWorkerPool(*this);
  mTaskMutex.lock();
  if(!task->hasDependencies()) {
    _taskReady(task);
  }
  mTaskMutex.unlock();
}

void WorkerPool::taskReady(std::shared_ptr<Task> task) {
  mTaskMutex.lock();
  _taskReady(task);
  mTaskMutex.unlock();
}

void WorkerPool::_taskReady(std::shared_ptr<Task> task) {
  //Tasks can be queued on queueTask or when its dependencies are done
  //Make sure not to add the task twice
  if(!task->hasBeenQueued()) {
    //Dependent task could have finished between adding dependency and queuing, in which case set the pool now
    task->setWorkerPool(*this);
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