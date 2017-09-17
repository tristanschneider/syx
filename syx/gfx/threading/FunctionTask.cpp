#include "Precompile.h"
#include "threading/FunctionTask.h"

FunctionTask::FunctionTask(std::function<void(void)> func, std::weak_ptr<TaskGroup> dependsOn, std::shared_ptr<TaskGroup> taskGroup)
  : Task(dependsOn, taskGroup)
  , mFunc(func) {
}

void FunctionTask::run() {
  mFunc();
}
