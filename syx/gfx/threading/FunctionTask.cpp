#include "Precompile.h"
#include "threading/FunctionTask.h"

FunctionTask::FunctionTask(std::function<void(void)> func)
  : mFunc(func) {
}

void FunctionTask::_run() {
  mFunc();
}
