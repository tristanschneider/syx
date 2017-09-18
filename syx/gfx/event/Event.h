#pragma once

enum class EventFlag : uint8_t {
  Invalid = 0,
  Graphics = 1 << 0,
  Physics = 1 << 1,
};
MAKE_BITWISE_ENUM(EventFlag);

enum class EventType : uint8_t {
  PhysicsCompUpdate
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

  std::vector<std::unique_ptr<Event>> mEvents;
  EventFlag mListenFlags;
  std::mutex mMutex;
};