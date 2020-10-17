#include "Precompile.h"
#include "event/DeferredEventBuffer.h"

#include <optional>

DeferredEventBuffer::~DeferredEventBuffer() = default;

DeferredEventBuffer::DeferCondition DeferredEventBuffer::Condition::waitTicks(uint64_t ticks) {
  return [startTick(std::optional<uint64_t>()), ticks](uint64_t currentTick) mutable {
    if(!startTick) {
      startTick = currentTick;
    }
    return currentTick - *startTick >= ticks;
  };
}

DeferredEventBuffer::DeferCondition DeferredEventBuffer::Condition::waitMS(std::chrono::milliseconds time) {
  return [startTime(std::chrono::steady_clock::now()), time](uint64_t) {
    return std::chrono::steady_clock::now() - startTime >= time;
  };
}

void DeferredEventBuffer::enqueueDeferredEvents(EventBuffer& buffer, uint64_t currentTick) {
  //Partition events that are ready for removal to the back, enqueue them, then remove
  auto enqueueBegin = std::partition(mEvents.begin(), mEvents.end(), [currentTick](const DeferredEvent& e) { return !e.mShouldEnqueue(currentTick); });
  std::for_each(enqueueBegin, mEvents.end(), [&buffer](DeferredEvent& event) {
    event.mEnqueue(buffer);
  });
  mEvents.erase(enqueueBegin, mEvents.end());
}
