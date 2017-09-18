#pragma once
class Task;
class TaskGroup;

class IWorkerPool {
public:
  virtual void queueTask(std::unique_ptr<Task> task) = 0;
  //Do work on this thread along with others until this group is done
  virtual void sync(std::weak_ptr<TaskGroup> group) = 0;
};