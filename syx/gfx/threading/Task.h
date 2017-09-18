#pragma once
class TaskGroup {
public:
  //Groups are kept alive by parent pointers, meaning order of events is bottom up, since parents won't start until all children are done
  TaskGroup(std::shared_ptr<TaskGroup> parent);

  static std::weak_ptr<TaskGroup> nullGroup();

private:
  std::shared_ptr<TaskGroup> mParent;
};

class Task {
public:
  //Tasks will only begin once dependsOn is gone
  //partOf is the task group this task is a part of, that other tasks point at using dependsOn
  //This is so that tasks can require other sets to complete before they start
  //A task can leave dependency as null if it doesn't matter when it completes
  Task(std::weak_ptr<TaskGroup> dependsOn, std::shared_ptr<TaskGroup> taskGroup);

  bool canRun();
  bool isLastInGroup();

  virtual void run() = 0;

private:
  std::weak_ptr<TaskGroup> mDependsOn;
  std::shared_ptr<TaskGroup> mTaskGroup;
};
