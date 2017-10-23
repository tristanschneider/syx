#pragma once
class Task;
class TaskGroup;

class IWorkerPool {
public:
  virtual void queueTask(std::shared_ptr<Task> task) = 0;
  //Do work on this thread along with others until this task is done
  virtual void sync(std::weak_ptr<Task> task) = 0;
  //Task will call this when it has no dependencies left to prevent it from starting
  virtual void taskReady(std::shared_ptr<Task> task) = 0;
};