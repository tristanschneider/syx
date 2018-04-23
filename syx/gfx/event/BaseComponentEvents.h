#pragma once
#include "event/Event.h"

class Component;
namespace Lua {
  class Node;
};

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

class SetComponentPropEvent : public Event {
public:
  //TODO: emplace such that no allocations are required to use this
  SetComponentPropEvent(Handle obj, size_t compType, const Lua::Node* prop, std::vector<uint8_t>&& buffer);
  SetComponentPropEvent(const SetComponentPropEvent& other);
  SetComponentPropEvent(SetComponentPropEvent&& other);
  ~SetComponentPropEvent();
  Handle mObj;
  size_t mCompType;
  const Lua::Node* mProp;
  std::vector<uint8_t> mBuffer;
};