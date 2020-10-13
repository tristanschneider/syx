#pragma once
#include "component/Component.h"
#include "event/Event.h"

class Component;
struct ComponentType;
struct IClaimedUniqueID;
namespace Lua {
  class Node;
  using NodeDiff = uint64_t;
};

class AddComponentEvent : public Event {
public:
  AddComponentEvent(Handle obj, size_t compType, size_t subType = 0);
  AddComponentEvent(Handle obj, const ComponentType& type);
  Handle mObj;
  size_t mCompType;
  size_t mSubType;
};

class RemoveComponentEvent : public Event {
public:
  RemoveComponentEvent(Handle obj, size_t compType, size_t subType = 0);
  RemoveComponentEvent(Handle obj, const ComponentType& type);
  Handle mObj;
  size_t mCompType;
  size_t mSubType;
};

class AddGameObjectEvent : public TypedEvent<AddGameObjectEvent> {
public:
  //Caller can specify an id that they claimed or instead leave it empty and the game system will generate a new one
  AddGameObjectEvent(Handle obj, std::shared_ptr<IClaimedUniqueID> uniqueID = nullptr);
  ~AddGameObjectEvent();
  Handle mObj;
  std::shared_ptr<IClaimedUniqueID> mUniqueID;
};

class RemoveGameObjectEvent : public Event {
public:
  RemoveGameObjectEvent(Handle obj);
  Handle mObj;
};

class SetComponentPropsEvent : public Event {
public:
  //TODO: emplace such that no allocations are required to use this
  SetComponentPropsEvent(Handle obj, ComponentType compType, const Lua::Node* prop, Lua::NodeDiff diff, std::vector<uint8_t>&& buffer);
  SetComponentPropsEvent(const SetComponentPropsEvent& other);
  SetComponentPropsEvent(SetComponentPropsEvent&& other);

  SetComponentPropsEvent& operator=(const SetComponentPropsEvent&) = delete;
  SetComponentPropsEvent& operator=(const SetComponentPropsEvent&&) = delete;

  ~SetComponentPropsEvent();
  Handle mObj;
  ComponentType mCompType;
  Lua::NodeDiff mDiff;
  const Lua::Node* mProp;
  std::vector<uint8_t> mBuffer;
};