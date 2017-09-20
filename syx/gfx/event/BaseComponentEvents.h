#pragma once
#include "event/Event.h"

class ComponentEvent : public Event {
public:
  ComponentEvent(EventType type, Handle obj, Handle compType)
    : Event(EventFlag::Component)
    , mEventType(type)
    , mObj(obj)
    , mCompType(compType) {
  }

  virtual Handle getHandle() const override {
    return static_cast<Handle>(mEventType);
  }

  virtual std::unique_ptr<Event> clone() const override {
    return std::make_unique<ComponentEvent>(mEventType, mObj, mCompType);
  }

  Handle mObj;
  Handle mCompType;
  EventType mEventType;
};