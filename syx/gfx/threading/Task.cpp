#include "Precompile.h"
#include "threading/Task.h"
#include "threading/IWorkerPool.h"

TaskDependency::TaskDependency()
  : mNextOther(nullptr) {
}

Task::Task()
  : mPool(nullptr) {
}

Task::~Task() {
  TaskDependency* d = mDependency.mNextOther;
  while(d) {
    TaskDependency* next = d->mNextOther;
    delete d;
    d = next;
  }
}

void Task::run() {
  _run();

  assert(mPool);
  //Now that this task is complete, remove it as a dependency for all tasks waiting on it
  while(mDependentHead) {
    TaskDependency d = mDependentHead->_removeDependency(*this);
    std::shared_ptr<Task> next = std::move(d.mNext);
    //Inform pool this dependent is ready to go if this was the last dependency
    if(!mDependentHead->hasDependencies()) {
      mPool->taskReady(mDependentHead);
    }

    mDependentHead = std::move(next);
  }
}

void Task::setWorkerPool(IWorkerPool& pool) {
  mPool = &pool;
}

void Task::addDependent(std::shared_ptr<Task> dependent) {
  //Get new node for dependent's dependencies to point at this
  TaskDependency* d = &dependent->mDependency;
  if(d->mDependency) {
    TaskDependency* old = d->mNextOther;
    d->mNextOther = new TaskDependency();
    d->mNextOther->mNextOther = old;
    d = d->mNextOther;
  }
  d->mDependency = shared_from_this();

  //Add to linked list of tasks depending on this
  if(mDependentHead) {
    std::shared_ptr<Task> oldHead = std::move(mDependentHead);
    mDependentHead = dependent;
    dependent->mDependency.mNext = oldHead;
  }
  else {
    mDependentHead = dependent;
  }
}

void Task::addDependency(std::shared_ptr<Task> dependency) {
  dependency->addDependent(shared_from_this());
}

TaskDependency Task::_removeDependency(const Task& parent) {
  TaskDependency* prev = nullptr;
  TaskDependency* cur = &mDependency;
  while(cur && cur->mDependency.get() != &parent) {
    prev = cur;
    cur = cur->mNextOther;
  }
  assert(cur); //This should always be found since the parent was pointing at this as a dependent

  TaskDependency result = *cur;
  if(prev) {
    prev->mNextOther = cur->mNextOther;
    delete cur;
  }
  else {
    //This is the head value stored on task, just clear the value
    cur->mDependency = nullptr;
  }
  return result;
}

bool Task::hasDependencies() {
  return mDependency.mDependency != nullptr || mDependency.mNext != nullptr;
}