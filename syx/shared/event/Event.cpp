#include "Precompile.h"
#include "event/Event.h"

EventListener::EventListener(size_t baseCapacity)
  : mBuffer(new uint8_t[baseCapacity])
  , mBufferSize(0)
  , mBufferCapacity(baseCapacity) {
}

void EventListener::push(Event&& e) {
  size_t start = mBufferSize;
  _growBuffer(e.getSize());
  Event::Registry::moveConstruct(std::move(e), mBuffer[start]);
}

void EventListener::push(const Event& e) {
  size_t start = mBufferSize;
  _growBuffer(e.getSize());
  Event::Registry::copyConstruct(e, mBuffer[start]);
}

void EventListener::registerEventHandler(size_t type, EventHandler h) {
  assert((!mEventHandlers.get(type) || !*mEventHandlers.get(type)) && "Only one listener per type allowed");
  mEventHandlers[type] = h;
}

void EventListener::registerGlobalHandler(EventHandler h) {
  assert((mGlobalHandler || !h) && "Only one global handler, so only valid to set it or clear it");
  mGlobalHandler = h;
}

void EventListener::appendTo(EventListener& listener) const {
  size_t curByte = 0;
  size_t dstStart = listener.mBufferSize;
  listener._growBuffer(mBufferSize);

  while(curByte < mBufferSize) {
    const Event& e = reinterpret_cast<const Event&>(mBuffer[curByte]);
    Event::Registry::copyConstruct(e, listener.mBuffer[dstStart + curByte]);
    curByte += e.getSize();
  }
}

void EventListener::handleEvents() {
  size_t curByte = 0;
  while(curByte < mBufferSize) {
    Event& e = reinterpret_cast<Event&>(mBuffer[curByte]);
    const EventHandler* handler = mEventHandlers.get(e.getType());

    if(handler && *handler)
      (*handler)(e);
    else if(mGlobalHandler)
      mGlobalHandler(e);

    curByte += e.getSize();
    e.~Event();
  }

  mBufferSize = 0;
}

void EventListener::clear() {
  size_t curByte = 0;
  while(curByte < mBufferSize) {
    Event& e = reinterpret_cast<Event&>(mBuffer[curByte]);
    curByte += e.getSize();
    e.~Event();
  }

  mBufferSize = 0;
}

void EventListener::_growBuffer(size_t bytes) {
  size_t newSize = mBufferSize + bytes;
  if(newSize < mBufferCapacity) {
    mBufferSize = newSize;
    return;
  }

  size_t newCap = std::max(1u, mBufferCapacity*2);
  while(newCap < newSize)
    newCap *= 2;
  uint8_t* newBuff = new uint8_t[newCap];

  //Move construct over to new buffer
  size_t curByte = 0;
  while(curByte < mBufferSize) {
    Event& e = reinterpret_cast<Event&>(mBuffer[curByte]);
    size_t nextByte = curByte + e.getSize();

    Event::Registry::moveConstruct(std::move(e), newBuff[curByte]);
    e.~Event();
    curByte = nextByte;
  }

  delete [] mBuffer;
  mBuffer = newBuff;
  mBufferSize = newSize;
  mBufferCapacity = newCap;
}

Event::Registry::Registry() {
}

void Event::Registry::registerEvent(size_t type, CopyConstructor copy, MoveConstructor move) {
  _get().mConstructors[type] = { std::move(copy), std::move(move) };
}

void Event::Registry::copyConstruct(const Event& e, uint8_t& buffer) {
  _get().mConstructors[e.getType()].first(e, &buffer);
}

void Event::Registry::moveConstruct(Event&& e, uint8_t& buffer) {
  _get().mConstructors[e.getType()].second(std::move(e), &buffer);
}

Event::Registry& Event::Registry::_get() {
  static Registry singleton;
  return singleton;
}

Event::Event(size_t type, size_t size)
  : mType(type)
  , mSize(size) {
}

Event::~Event() {
}

size_t Event::getSize() const {
  return mSize;
}

size_t Event::getType() const {
  return mType;
}