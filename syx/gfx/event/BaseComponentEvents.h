#pragma once
#include "event/Event.h"

class AddComponentEvent : public Event {
public:
  AddComponentEvent(Handle obj, Handle compType);
  Handle mObj;
  Handle mCompType;
};

class RemoveComponentEvent : public Event {
public:
  RemoveComponentEvent(Handle obj, Handle compType);
  Handle mObj;
  Handle mCompType;
};
