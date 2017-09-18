#include "threading/IWorkerPool.h"

class Task;
class TaskGroup;

class WorkerPool {
public:
  WorkerPool(size_t threadCount);
  ~WorkerPool();

  void queueTask(std::unique_ptr<Task> task);
  //Do work on this thread along with others until this group is done
  void sync(std::weak_ptr<TaskGroup> group);

private:
  void _workerLoop();
  std::unique_ptr<Task> _getTask();
  void _doNextTask(std::unique_lock<std::mutex>& taskLock, bool& wakeWorkers);

  std::vector<std::unique_ptr<Task>> mTasks;
  std::mutex mTaskMutex;

  std::condition_variable mWorkerCV;
  bool mTerminate;

  std::thread* mWorkers;
  size_t mWorkerCount;
};