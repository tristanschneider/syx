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

DEFINE_EVENT(SetComponentPropsEvent, Handle obj, size_t compType, const Lua::Node* prop, Lua::NodeDiff diff, std::vector<uint8_t>&& buffer)
  , mObj(obj)
  , mCompType(compType)
  , mDiff(diff)
  , mProp(prop)
  , mBuffer(std::move(buffer)) {
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
    mProp->destructBuffer(&mBuffer[0]);
  }
}
