#include "Precompile.h"
#include "threading/WorkerPool.h"
#include "threading/Task.h"

WorkerPool::WorkerPool(size_t workerCount)
  : mTerminate(false)
  , mSyncing(false)
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

void WorkerPool::sync(std::weak_ptr<Task> task) {
  {
    std::shared_ptr<Task> t = task.lock();
    if(t)
      mSyncTask = t.get();
    else
      return;
  }
  mSyncing = true;
  //Normally there shouldn't be termination during sync because the syncing thread is probably the one who would tear down
  while(mSyncing) {
    std::unique_lock<std::mutex> taskLock(mTaskMutex);
    //task.expired shouldn't generally happen, but is possible since sync check isn't in a locked section
    if(!mSyncing || mTerminate || task.expired())
      break;
    mWorkerCV.wait(taskLock);
  }

  mSyncing = false;
  mSyncTask = nullptr;
}

void WorkerPool::_wakeSyncer() {
  mTaskMutex.lock();
  mSyncing = false;
  mWorkerCV.notify_all();
  mTaskMutex.unlock();
}

void WorkerPool::_workerLoop() {
  while(!mTerminate) {
    //Use same lock for condition variable as tasks, so if work is being queued,
    //a thread doesn't miss the notification then immediately wait on the condition variable
    std::unique_lock<std::mutex> taskLock(mTaskMutex);
    _doNextTask(taskLock);
  }
}

void WorkerPool::_doNextTask(std::unique_lock<std::mutex>& taskLock) {
  std::shared_ptr<Task> task = _getTask();
  if(task) {
    taskLock.unlock();
    task->run();
    Task* taskAddr = task.get();
    task = nullptr;
    if(mSyncing && mSyncTask == taskAddr) {
      _wakeSyncer();
    }
  }
  else {
    //Termination might have been signaled as we were grabbing mutex, check before sleeping
    if(!mTerminate) {
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