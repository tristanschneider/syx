#include "Precompile.h"
#include "event/BaseComponentEvents.h"

#include "component/Component.h"
#include "lua/LuaNode.h"

DEFINE_EVENT(AddComponentEvent, Handle obj, Handle compType)
  , mObj(obj)
  , mCompType(compType) {
}

DEFINE_EVENT(RemoveComponentEvent, Handle obj, Handle compType)
  , mObj(obj)
  , mCompType(compType) {
}

DEFINE_EVENT(AddGameObjectEvent, Handle obj)
  , mObj(obj) {
}

DEFINE_EVENT(RemoveGameObjectEvent, Handle obj)
  , mObj(obj) {
}

DEFINE_EVENT(SetComponentPropsEvent, std::unique_ptr<Component> comp)
  , mNewValue(std::move(comp)) {
}

SetComponentPropsEvent::SetComponentPropsEvent(const SetComponentPropsEvent& other)
  : Event(Event::typeId<SetComponentPropsEvent>(), sizeof(SetComponentPropsEvent))
  , mNewValue(other.mNewValue->clone()) {
}

SetComponentPropsEvent::SetComponentPropsEvent(SetComponentPropsEvent&& other)
  : Event(Event::typeId<SetComponentPropsEvent>(), sizeof(SetComponentPropsEvent))
  , mNewValue(std::move(other.mNewValue)) {
}

SetComponentPropsEvent::~SetComponentPropsEvent() {
}

DEFINE_EVENT(SetComponentPropEvent, Handle obj, size_t compType, const Lua::Node* prop, std::vector<uint8_t>&& buffer)
  , mObj(obj)
  , mCompType(compType)
  , mProp(prop)
  , mBuffer(std::move(buffer)) {
}

SetComponentPropEvent::SetComponentPropEvent(const SetComponentPropEvent& other) 
  : Event(Event::typeId<SetComponentPropEvent>(), sizeof(SetComponentPropEvent)) {
  mObj = other.mObj;
  mCompType = other.mCompType;
  mProp = other.mProp;
  mBuffer.resize(other.mBuffer.size());
  mProp->copyConstructBufferToBuffer(&other.mBuffer[0], &mBuffer[0]);
}

SetComponentPropEvent::SetComponentPropEvent(SetComponentPropEvent&& other)
  : Event(Event::typeId<SetComponentPropEvent>(), sizeof(SetComponentPropEvent))
  , mObj(other.mObj)
  , mCompType(other.mCompType)
  , mProp(other.mProp)
  , mBuffer(std::move(other.mBuffer)) {
  other.mProp = nullptr;
}

SetComponentPropEvent::~SetComponentPropEvent() {
  if(mProp) {
    mProp->destructBuffer(&mBuffer[0]);
  }
}
