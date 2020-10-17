#pragma once
#include "event/Event.h"
#include "event/EventBuffer.h"

//This class provides a container to queue events for a later frame
//Ideally EventBuffer would be a template that could be extended to do this, but this'll do for now
//Another possibility would be if WorkerPool was extended to allow queueing callbacks freely enough to support the functionality here.
//Elements will be added to the EventBuffer when they are ready, based on DeferCondition returning true
//Relative order between multiple deferred events is not guaranteed even if they complete on the same frame
class DeferredEventBuffer {
public:
  using DeferCondition = std::function<bool(uint64_t)>;

  //Convenience methods for common wait conditions
  struct Condition {
    //Wait until the given number of ticks has passed, 0 meaning the next tick that adds deferred events
    static DeferCondition waitTicks(uint64_t ticks);
    //Wait until the given amount of time has elapsed
    static DeferCondition waitMS(std::chrono::milliseconds time);
  };

  ~DeferredEventBuffer();

  //Don't want to expose the unique_ptr copy directly since it could be optimized out in the future
  template<class EventType>
  void push(EventType event, DeferCondition shouldEnqueue) {
    mEvents.push_back({ std::move(shouldEnqueue), [event(std::move(event))](EventBuffer& buffer) mutable {
      buffer.push(std::move(event));
    }});
  }

  //Add all events that are ready to be enqueued to the buffer
  //These events are then removed from the DeferredEventBuffer
  //The tick value is an arbitrary value as convenience to functions that want to delay based on ticks
  void enqueueDeferredEvents(EventBuffer& buffer, uint64_t currentTick);

private:
  struct DeferredEvent {
    DeferCondition mShouldEnqueue;
    //EventBuffer requires the derived type to be known when called, so wrap this type in a std function to call
    std::function<void(EventBuffer&)> mEnqueue;
  };
  std::vector<DeferredEvent> mEvents;
};