#include "Precompile.h"
#include "component/Component.h"
#include "system/MessagingSystem.h"
#include "event/BaseComponentEvents.h"

Component::Component(Handle type, Handle owner, MessagingSystem* messaging)
  : mOwner(owner)
  , mType(type)
  , mMessaging(messaging) {
  if(mMessaging)
    mMessaging->fireEvent(std::make_unique<ComponentEvent>(EventType::AddComponent, mOwner, getHandle()));
}

Component::~Component() {
  if(mMessaging)
    mMessaging->fireEvent(std::make_unique<ComponentEvent>(EventType::RemoveComponent, mOwner, getHandle()));
}