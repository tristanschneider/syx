#pragma once
#include "threading/Task.h"

class FunctionTask : public Task {
public:
    FunctionTask(std::function<void(void)> func, std::weak_ptr<TaskGroup> dependsOn, std::shared_ptr<TaskGroup> taskGroup);

    virtual void run() override;

private:
  std::function<void(void)> mFunc;
};