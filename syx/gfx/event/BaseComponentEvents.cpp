#include "Precompile.h"
#include "event/BaseComponentEvents.h"

#include "component/Component.h"

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
