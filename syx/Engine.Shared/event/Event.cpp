#include "Precompile.h"
#include "event/Event.h"

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

typeId_t<Event> Event::getType() const {
  return mType;
}