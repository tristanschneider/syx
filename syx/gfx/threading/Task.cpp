#include "Precompile.h"
#include "threading/Task.h"

Task::Task(std::weak_ptr<TaskGroup> dependsOn, std::shared_ptr<TaskGroup> taskGroup)
  : mDependsOn(dependsOn)
  , mTaskGroup(taskGroup) {
}

bool Task::canRun() {
  return mDependsOn.expired();
}

bool Task::isLastInGroup() {
  return mTaskGroup.unique();
}
