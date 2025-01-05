#pragma once

#include "AppBuilder.h"
#include "ThreadLocals.h"

namespace enki {
  struct TaskSetPartition;
}

class GameTaskArgs : public AppTaskArgs {
public:
  GameTaskArgs();
  GameTaskArgs(enki::TaskSetPartition range, const ThreadLocalData& data, size_t thread);
  GameTaskArgs(enki::TaskSetPartition range, ThreadLocals& locals, size_t threadIdx);
  Tasks::ILocalScheduler* getScheduler() final;
  RuntimeDatabase& getLocalDB() final;
  std::unique_ptr<AppTaskArgs> clone() const final;
  IRandom* getRandom() final;
  IDBEvents* getEvents() final;

private:
  ThreadLocalData tld;
};