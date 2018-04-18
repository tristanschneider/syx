#pragma once
#include "event/Event.h"

class Component;

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

class SetComponentPropsEvent : public Event {
public:
  SetComponentPropsEvent(std::unique_ptr<Component> comp);
  SetComponentPropsEvent(const SetComponentPropsEvent& other);
  SetComponentPropsEvent(SetComponentPropsEvent&& other);
  ~SetComponentPropsEvent();
  std::unique_ptr<Component> mNewValue;
};