#pragma once
class Task;

class IWorkerPool {
public:
  virtual void queueTask(std::unique_ptr<Task> task) = 0;
};