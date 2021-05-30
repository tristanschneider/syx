#include "Precompile.h"
#include "event/BaseComponentEvents.h"

#include "component/Component.h"
#include "lua/LuaNode.h"
#include "registry/IDRegistry.h"

DEFINE_EVENT(AddComponentEvent, Handle obj, size_t compType, size_t subType)
  , mObj(obj)
  , mCompType(compType)
  , mSubType(subType) {
}

AddComponentEvent::AddComponentEvent(Handle obj, const ComponentType& type)
  : Event(typeId<AddComponentEvent>(), sizeof(AddComponentEvent))
  , mObj(obj)
  , mCompType(type.id)
  , mSubType(type.subId) {
}

DEFINE_EVENT(RemoveComponentEvent, Handle obj, size_t compType, size_t subType)
  , mObj(obj)
  , mCompType(compType)
  , mSubType(subType) {
}

RemoveComponentEvent::RemoveComponentEvent(Handle obj, const ComponentType& type)
  : Event(typeId<RemoveComponentEvent>(), sizeof(RemoveComponentEvent))
  , mObj(obj)
  , mCompType(type.id)
  , mSubType(type.subId) {
}

AddGameObjectEvent::AddGameObjectEvent(Handle obj, std::shared_ptr<IClaimedUniqueID> uniqueID)
  : mObj(obj)
  , mUniqueID(uniqueID) {
}

AddGameObjectEvent::~AddGameObjectEvent() = default;

DEFINE_EVENT(RemoveGameObjectEvent, Handle obj)
  , mObj(obj) {
}

DEFINE_EVENT(SetComponentPropsEvent, Handle obj, ComponentType compType, const Lua::Node* prop, Lua::NodeDiff diff, std::vector<uint8_t>&& buffer, size_t fromSystem)
  , mObj(obj)
  , mCompType(compType)
  , mDiff(diff)
  , mProp(prop)
  , mBuffer(std::move(buffer))
  , mFromSystem(fromSystem) {
}

SetComponentPropsEvent::SetComponentPropsEvent(const SetComponentPropsEvent& other) 
  : Event(Event::typeId<SetComponentPropsEvent>(), sizeof(SetComponentPropsEvent)) {
  mObj = other.mObj;
  mCompType = other.mCompType;
  mDiff = other.mDiff;
  mProp = other.mProp;
  mBuffer.resize(other.mBuffer.size());
  mProp->copyConstructBufferToBuffer(&other.mBuffer[0], &mBuffer[0]);
}

SetComponentPropsEvent::SetComponentPropsEvent(SetComponentPropsEvent&& other)
  : Event(Event::typeId<SetComponentPropsEvent>(), sizeof(SetComponentPropsEvent))
  , mObj(other.mObj)
  , mCompType(other.mCompType)
  , mDiff(other.mDiff)
  , mProp(other.mProp)
  , mBuffer(std::move(other.mBuffer)) {
  other.mProp = nullptr;
}

SetComponentPropsEvent::~SetComponentPropsEvent() {
  if(mProp) {
    mProp->destructBuffer(mBuffer.data());
  }
}
