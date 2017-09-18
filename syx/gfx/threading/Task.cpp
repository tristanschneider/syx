#include "Precompile.h"
#include "threading/Task.h"

TaskGroup::TaskGroup(std::shared_ptr<TaskGroup> parent)
  : mParent(parent) {
}

std::weak_ptr<TaskGroup> TaskGroup::nullGroup() {
  return std::weak_ptr<TaskGroup>();
}

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
