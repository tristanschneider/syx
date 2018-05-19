#pragma once
#include "event/Event.h"

class Component;
namespace Lua {
  class Node;
  using NodeDiff = uint64_t;
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
  //TODO: emplace such that no allocations are required to use this
  SetComponentPropsEvent(Handle obj, size_t compType, const Lua::Node* prop, Lua::NodeDiff diff, std::vector<uint8_t>&& buffer);
  SetComponentPropsEvent(const SetComponentPropsEvent& other);
  SetComponentPropsEvent(SetComponentPropsEvent&& other);
  ~SetComponentPropsEvent();
  Handle mObj;
  size_t mCompType;
  Lua::NodeDiff mDiff;
  const Lua::Node* mProp;
  std::vector<uint8_t> mBuffer;
};