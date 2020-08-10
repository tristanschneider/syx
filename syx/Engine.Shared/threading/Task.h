#pragma once
#include <atomic>

class Task;
class IWorkerPool;

enum class TaskState : uint8_t {
  //Default state. Either not added to worker pool or waiting on dependencies
  Waiting,
  //If a task is Waiting then hasWorkerPool determines if it has been queued to a pool or not, which is NOT the same as Queued below
  //All dependencies complete, in queue to be picked up by a worker
  Queued,
  Done
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
  bool hasWorkerPool() const;
  //dependent depends on this
  void addDependent(std::shared_ptr<Task> dependent);
  //this depends on dependency
  void addDependency(std::shared_ptr<Task> dependency);
  //Compose dependencies like a.then(b).then(c)
  std::shared_ptr<Task> then(std::shared_ptr<Task> next);
  bool hasDependencies();
  void setQueued();
  bool hasBeenQueued();
  TaskState getState() const;

protected:
  virtual void _run() {}

  //Number of uncompleted tasks this still depends on
  std::atomic_int mDependencies;
  std::atomic<TaskState> mState;
  //All tasks that depend on this
  std::vector<Task*> mDependents;
  std::mutex mDependentsMutex;
  //Objects keep themselves alive while waiting to be executed with this reference
  //It is reasonable for the user to be holding a weak ptr to this task to be adding dependencies while it's in the pool
  std::shared_ptr<Task> mSelf;
  IWorkerPool* mPool;
};
