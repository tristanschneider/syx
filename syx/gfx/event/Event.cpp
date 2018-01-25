#include "Precompile.h"
#include "event/Event.h"

namespace Test {

EventListener::EventListener() {
}

void EventListener::push(const Event& e) {
  size_t start = mBuffer.size();
  size_t reqSize = start + e.getSize();
  mBuffer.resize(reqSize);
  Event::Registry::construct(e, mBuffer[start]);
}

void EventListener::registerEventHandler(size_t type, EventHandler h) {
  assert(!mEventHandlers.get(type) && "Only one listener per type allowed");
  mEventHandlers[type] = h;
}

void EventListener::registerGlobalHandler(EventHandler h) {
  assert(mGlobalHandler || !h); //Only one global handler, so only valid to set it or clear it
  mGlobalHandler = h;
}

void EventListener::appendTo(EventListener& listener) const {
  size_t curByte = 0;
  std::vector<uint8_t>& dstBuff = listener.mBuffer;
  size_t dstStart = dstBuff.size();
  dstBuff.resize(dstStart + mBuffer.size());

  while(curByte < mBuffer.size()) {
    const Event& e = reinterpret_cast<const Event&>(mBuffer[curByte]);
    Event::Registry::construct(e, dstBuff[dstStart + curByte]);
    curByte += e.getSize();
  }
}

void EventListener::handleEvents() {
  size_t curByte = 0;
  while(curByte < mBuffer.size()) {
    Event& e = reinterpret_cast<Event&>(mBuffer[curByte]);
    const EventHandler* handler = mEventHandlers.get(e.getType());

    if(const EventHandler* handler = mEventHandlers.get(e.getType()))
      (*handler)(e);
    else if(mGlobalHandler)
      mGlobalHandler(e);

    curByte += e.getSize();
    e.~Event();
  }

  mBuffer.clear();
}


Event::Registry::Registry() {
}

void Event::Registry::registerEvent(size_t type, Constructor c) {
  _get().mConstructors[type] = std::move(c);
}

void Event::Registry::construct(const Event& e, uint8_t& buffer) {
  _get().mConstructors[e.getType()](e, &buffer);
}

Event::Registry& Event::Registry::_get() {
  static Registry singleton;
  return singleton;
}

void TestEvents() {
  TestEvent ta(1);
  TestEvent2 tb(1, true, 'c');
  EventListener la, lb;
  la.push(ta);
  la.push(tb);

  la.appendTo(lb);

  lb.registerEventHandler(Event::typeId<TestEvent>(), [](const Event& e) {
    auto& te = static_cast<const TestEvent&>(e);
    printf("Handled TestEvent %i\n", te.mA);
  });
  lb.registerEventHandler(Event::typeId<TestEvent2>(), [](const Event& e) {
    auto& te = static_cast<const TestEvent2&>(e);
    printf("Handled TestEvent2 %i\n", te.mA);
  });

  lb.handleEvents();
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

DEFINE_EVENT(TestEvent, int a)
  , mA(a) {
}

DEFINE_EVENT(TestEvent2, int a, bool b, char c)
  , mA(a)
  , mB(b)
  , mC(c) {
}
}