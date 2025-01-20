#pragma once

#include "ITaskImpl.h"

class ChainedTaskImpl : public ITaskImpl {
public:
  ChainedTaskImpl(std::unique_ptr<ITaskImpl> p);
  ~ChainedTaskImpl();
  virtual void setWorkerCount(size_t count);
  virtual AppTaskMetadata init(RuntimeDatabase& db);
  virtual void initThreadLocal(AppTaskArgs& args);
  virtual void execute(AppTaskArgs& args);
  virtual std::shared_ptr<AppTaskConfig> getConfig();
  virtual AppTaskPinning::Variant getPinning();
private:
  std::unique_ptr<ITaskImpl> parent;
};