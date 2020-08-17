#include "Precompile.h"
#include "threading/Task.h"
#include "threading/IWorkerPool.h"

Task::Task()
  : mPool(nullptr)
  , mState(TaskState::Waiting)
  , mDependencies(0) {
}

Task::~Task() {
}

void Task::run() {
  _run();
  mState = TaskState::Done;

  assert(mPool);
  //RW lock would be good here, as it's only needed to protect against adding dependents while completing
  mDependentsMutex.lock();
  //Now that this task is complete, remove it as a dependency for all tasks waiting on it
  for(Task* dep : mDependents) {
    //If this was the last dependency on the task, it can run now
    const int prevDependencyCount = dep->mDependencies.fetch_sub(1);
    assert(prevDependencyCount > 0 && "Dependency count should always be positive otherwise bookeeping has failed");
    if(prevDependencyCount == 1) {
      mPool->taskReady(dep->shared_from_this());
    }
  }
  mDependentsMutex.unlock();

  mSelf = nullptr;
}

void Task::setWorkerPool(IWorkerPool& pool) {
  mPool = &pool;
  if(mState != TaskState::Done) {
    //Tasks manage their own lifetime while waiting in the pool, and drop this reference on completion
    mSelf = shared_from_this();
  }
}

bool Task::hasWorkerPool() const {
  return mPool != nullptr;
}

void Task::addDependent(std::shared_ptr<Task> dependent) {
  //If this is already done, then there's no need to add the dependency, since there's nothing for dependent to wait for
  if(mState == TaskState::Done)
    return;

  //RW lock would be great here as this is only needed if the task finishes while a dependency is added
  mDependentsMutex.lock();
  if(mState != TaskState::Done) {
    dependent->mDependencies.fetch_add(1);
    mDependents.push_back(dependent.get());
  }
  mDependentsMutex.unlock();

  //If the task completed while we were adding to dependents no need to remove it as it won't be touched after completion
}

void Task::addDependency(std::shared_ptr<Task> dependency) {
  dependency->addDependent(shared_from_this());
}

std::shared_ptr<Task> Task::then(std::shared_ptr<Task> next) {
  addDependent(next);
  return next;
}

bool Task::hasDependencies() {
  return mDependencies != 0;
}

void Task::setQueued() {
  //Won't do anything if already queued
  TaskState expected = TaskState::Waiting;
  mState.compare_exchange_strong(expected, TaskState::Queued);
}

bool Task::hasBeenQueued() {
  TaskState state = mState;
  switch(state) {
    case TaskState::Queued:
    case TaskState::Done:
      return true;
    default:
      return false;
  }
}

TaskState Task::getState() const {
  return mState;
}
