#include "threading/IWorkerPool.h"

class Task;

class WorkerPool {
public:
  WorkerPool(size_t threadCount);
  ~WorkerPool();

  void queueTask(std::unique_ptr<Task> task);

private:
  void _workerLoop();
  std::unique_ptr<Task> _getTask();
  void _wakeOneWorker();
  void _wakeAllWorkers();

  std::vector<std::unique_ptr<Task>> mTasks;
  std::mutex mTaskMutex;

  std::condition_variable mWorkerCV;
  std::mutex mWorkerCVMutex;
  bool mTerminate;

  std::thread* mWorkers;
  size_t mWorkerCount;
};