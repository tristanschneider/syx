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

class AddGameObjectEvent : public Event {
public:
  AddGameObjectEvent(Handle obj);
  Handle mObj;
};

class RemoveGameObjectEvent : public Event {
public:
  RemoveGameObjectEvent(Handle obj);
  Handle mObj;
};