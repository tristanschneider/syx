#pragma once

class Event;

class EventBufferConstIt {
public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = Event;
  using different_type = std::ptrdiff_t;
  using pointer = Event*;
  using reference = Event&;

  EventBufferConstIt(const uint8_t* ptr);

  EventBufferConstIt& operator++();
  EventBufferConstIt operator++(int);
  bool operator==(EventBufferConstIt it);
  bool operator!=(EventBufferConstIt it);
  const Event& operator*() const;
  const Event* operator->() const;

private:
  const uint8_t* mPtr;
};

class EventBuffer {
public:

  EventBuffer(size_t baseCapacity = 256);
  //No reason to copy this, so any copies would likely be accidental
  EventBuffer(const EventBuffer&) = delete;
  EventBuffer& operator=(const EventBuffer&) = delete;

  template<class EventType>
  void push(EventType e) {
    static_assert(std::is_convertible_v<EventType, Event>, "Should be an event type");
    size_t start = mBufferSize;
    _growBuffer(e.getSize());
    Event::Registry::moveConstruct(std::move(e), mBuffer[start]);
  }

  //Emplace an event of the given type and size. Must still be an event, but allows overallocation for variable event sizes within the same event type
  template<typename E, typename... Args>
  void emplace(size_t size, Args&&... args) {
    size_t start = mBufferSize;
    _growBuffer(size);
    new (&mBuffer[start]) E(std::forward(args)...);
  }

  void appendTo(EventBuffer& listener) const;
  void clear();
  EventBufferConstIt begin() const;
  EventBufferConstIt end() const;

private:
  void _growBuffer(size_t bytes);

  uint8_t* mBuffer;
  size_t mBufferSize;
  size_t mBufferCapacity;
};