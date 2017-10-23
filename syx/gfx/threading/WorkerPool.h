#include "threading/IWorkerPool.h"

class Task;

class WorkerPool : public IWorkerPool {
public:
  WorkerPool(size_t threadCount);
  ~WorkerPool();

  void queueTask(std::shared_ptr<Task> task) override;
  void sync(std::weak_ptr<Task> task) override;
  void taskReady(std::shared_ptr<Task> task) override;

private:
  void _workerLoop();
  std::shared_ptr<Task> _getTask();
  void _doNextTask(std::unique_lock<std::mutex>& taskLock);
  void _wakeSyncer();

  std::vector<std::shared_ptr<Task>> mTasks;
  std::mutex mTaskMutex;

  std::condition_variable mWorkerCV;
  bool mTerminate;
  bool mSyncing;
  std::weak_ptr<Task> mSyncTask;

  std::thread* mWorkers;
  size_t mWorkerCount;
};