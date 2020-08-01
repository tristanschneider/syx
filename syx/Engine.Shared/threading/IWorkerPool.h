#pragma once
class Task;
class TaskGroup;

class IWorkerPool {
public:
  virtual ~IWorkerPool() = default;
  // Frienship as the task calls taskReady for dependencies upon completion
  friend class Task;

  virtual void queueTask(std::shared_ptr<Task> task) = 0;

protected:
  //Task will call this when it has no dependencies left to prevent it from starting
  virtual void taskReady(std::shared_ptr<Task> task) = 0;
};