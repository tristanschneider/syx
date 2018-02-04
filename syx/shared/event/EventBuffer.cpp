#include "Precompile.h"
#include "event/EventBuffer.h"
#include "event/Event.h"

EventBufferConstIt::EventBufferConstIt(const uint8_t* ptr)
  : mPtr(ptr) {
}

EventBufferConstIt& EventBufferConstIt::operator++() {
  mPtr += (**this).getSize();
  return *this;
}

EventBufferConstIt EventBufferConstIt::operator++(int) {
  EventBufferConstIt copy = *this;
  ++(*this);
  return copy;
}

bool EventBufferConstIt::operator==(EventBufferConstIt it) {
  return mPtr == it.mPtr;
}

bool EventBufferConstIt::operator!=(EventBufferConstIt it) {
  return mPtr != it.mPtr;
}

const Event& EventBufferConstIt::operator*() {
  return reinterpret_cast<const Event&>(*mPtr);
}

EventBuffer::EventBuffer(size_t baseCapacity)
  : mBuffer(new uint8_t[baseCapacity])
  , mBufferSize(0)
  , mBufferCapacity(baseCapacity) {
}

void EventBuffer::push(Event&& e) {
  size_t start = mBufferSize;
  _growBuffer(e.getSize());
  Event::Registry::moveConstruct(std::move(e), mBuffer[start]);
}

void EventBuffer::push(const Event& e) {
  size_t start = mBufferSize;
  _growBuffer(e.getSize());
  Event::Registry::copyConstruct(e, mBuffer[start]);
}

void EventBuffer::clear() {
  size_t curByte = 0;
  while(curByte < mBufferSize) {
    Event& e = reinterpret_cast<Event&>(mBuffer[curByte]);
    curByte += e.getSize();
    e.~Event();
  }

  mBufferSize = 0;
}

void EventBuffer::_growBuffer(size_t bytes) {
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

void EventBuffer::appendTo(EventBuffer& listener) const {
  size_t curByte = 0;
  size_t dstStart = listener.mBufferSize;
  listener._growBuffer(mBufferSize);

  while(curByte < mBufferSize) {
    const Event& e = reinterpret_cast<const Event&>(mBuffer[curByte]);
    Event::Registry::copyConstruct(e, listener.mBuffer[dstStart + curByte]);
    curByte += e.getSize();
  }
}

EventBufferConstIt EventBuffer::begin() const {
  return EventBufferConstIt(mBuffer);
}

EventBufferConstIt EventBuffer::end() const {
  return EventBufferConstIt(mBuffer + mBufferSize);
}
