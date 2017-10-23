#pragma once
class Task;
class IWorkerPool;

struct TaskDependency {
  TaskDependency();

  //The task we're depending on
  std::shared_ptr<Task> mDependency;
  //Next in a linked list of other tasks that also depend on mDependency
  std::shared_ptr<Task> mNext;
  //Other dependencies for this task
  TaskDependency* mNextOther;
};

class Task : public std::enable_shared_from_this<Task> {
public:
  Task();
  Task(const Task&) = delete;
  Task(const Task&&) = delete;
  Task& operator=(const Task&) = delete;
  virtual ~Task();

  void run();
  void setWorkerPool(IWorkerPool& pool);
  void addDependent(std::shared_ptr<Task> dependent);
  void addDependency(std::shared_ptr<Task> dependency);
  bool hasDependencies();

protected:
  virtual void _run() {}
  //Remove the appropriate dependency from this task's list and return the removed value.
  TaskDependency _removeDependency(const Task& parent);

  //What this depends on, which contains a linked list
  //Keep head by value because one dependency is way more common than multiple, so save the cache miss
  TaskDependency mDependency;
  //Head of linked list of tasks that depend on this
  std::shared_ptr<Task> mDependentHead;
  IWorkerPool* mPool;
};
