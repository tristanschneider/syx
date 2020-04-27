#include "Precompile.h"
#include "component/ComponentPublisher.h"

#include "component/Component.h"
#include "event/BaseComponentEvents.h"
#include "lua/LuaNode.h"
#include "provider/MessageQueueProvider.h"
#include "system/LuaGameSystem.h"

ComponentPublisher::ComponentPublisher(const Component& component)
  : mComponent(&component) {
}

const Component* ComponentPublisher::get() const {
  return mComponent;
}

const Component& ComponentPublisher::operator*() const {
  return *mComponent;
}

const Component* ComponentPublisher::operator->() const {
  return mComponent;
}

void ComponentPublisher::publish(const Component& component, lua_State* l) const {
  publish(component, LuaGameSystem::check(l).getMessageQueueProvider());
}

void ComponentPublisher::publish(const Component& component, MessageQueueProvider& msg) const {
  assert(component.getType() == mComponent->getType() && "Publisher should only be used on the same component type");
  if(const Lua::Node* props = component.getLuaProps(); props && component.getType() == mComponent->getType()) {
    if(const Lua::NodeDiff diff = props->getDiff(mComponent, &component)) {
      std::vector<uint8_t> buff(props->size());
      props->copyConstructToBuffer(&component, buff.data());
      msg.getMessageQueue().get().push(SetComponentPropsEvent(component.getOwner(), component.getFullType(), props, diff, std::move(buff)));
    }
  }
}