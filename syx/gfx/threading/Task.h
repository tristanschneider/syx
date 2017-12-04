#pragma once
class Task;
class IWorkerPool;

enum class TaskState : uint8_t {
  Waiting,
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
  void addDependent(std::shared_ptr<Task> dependent);
  void addDependency(std::shared_ptr<Task> dependency);
  bool hasDependencies();
  void setQueued();
  bool hasBeenQueued();

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
