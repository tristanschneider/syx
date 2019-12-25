#pragma once
#include "threading/Task.h"

class FunctionTask : public Task {
public:
  FunctionTask(std::function<void(void)> func);

protected:
  virtual void _run() override;

private:
  std::function<void(void)> mFunc;
};