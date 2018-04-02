#include "threading/IWorkerPool.h"

class Task;

class WorkerPool : public IWorkerPool {
public:
  WorkerPool(size_t threadCount);
  ~WorkerPool();

  void queueTask(std::shared_ptr<Task> task) override;

protected:
  void taskReady(std::shared_ptr<Task> task) override;

private:
  void _taskReady(std::shared_ptr<Task> task);
  void _workerLoop();
  std::shared_ptr<Task> _getTask();

  std::vector<std::shared_ptr<Task>> mTasks;
  std::mutex mTaskMutex;

  std::condition_variable mWorkerCV;
  bool mTerminate;

  std::thread* mWorkers;
  size_t mWorkerCount;
};