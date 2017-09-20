#pragma once

enum class EventFlag : uint8_t {
  Invalid = 0,
  Graphics = 1 << 0,
  Physics = 1 << 1,
  Component = 1 << 2
};
MAKE_BITWISE_ENUM(EventFlag);

enum class EventType : uint8_t {
  PhysicsCompUpdate,
  RenderableUpdate,
  AddComponent,
  RemoveComponent
};

class Event {
public:
  Event(EventFlag flags)
    : mFlags(flags) {
  }

  //Handle describes the specific event type
  virtual Handle getHandle() const = 0;
  virtual std::unique_ptr<Event> clone() const = 0;

  //Flags describe event category
  EventFlag getFlags() const {
    return mFlags;
  }

protected:
  EventFlag mFlags;
};

struct EventListener {
  EventListener(EventFlag listenFlags)
    : mListenFlags(listenFlags) {
  }

  void updateLocal() {
    mLocalEvents.clear();
    mMutex.lock();
    mLocalEvents.swap(mEvents);
    mMutex.unlock();
  }

  std::vector<std::unique_ptr<Event>> mEvents;
  //Local buffer used to spend as little time as possible locking event queues
  std::vector<std::unique_ptr<Event>> mLocalEvents;
  EventFlag mListenFlags;
  std::mutex mMutex;
};