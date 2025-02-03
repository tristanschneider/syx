#include "Precompile.h"
#include "GameTaskArgs.h"

#include "Scheduler.h"

GameTaskArgs::GameTaskArgs() = default;

GameTaskArgs::GameTaskArgs(enki::TaskSetPartition range, const ThreadLocalData& data, size_t thread)
  : tld{ data }
{
  begin = range.start;
  end = range.end;
  threadIndex = thread;
}

GameTaskArgs::GameTaskArgs(enki::TaskSetPartition range, ThreadLocals& locals, size_t threadIdx)
  : GameTaskArgs(range, locals.get(threadIdx), threadIdx)
{
}

Tasks::ILocalScheduler* GameTaskArgs::getScheduler() {
  return tld.scheduler;
}

RuntimeDatabase& GameTaskArgs::getLocalDB() {
  return *tld.statEffects;
}

std::unique_ptr<AppTaskArgs> GameTaskArgs::clone() const {
  return std::make_unique<GameTaskArgs>(
    enki::TaskSetPartition{ static_cast<uint32_t>(begin), static_cast<uint32_t>(end) },
    tld,
    threadIndex
  );
}

IRandom* GameTaskArgs::getRandom() {
  return tld.random;
}
