#include "Precompile.h"
#include "component/Component.h"
#include "system/MessagingSystem.h"
#include "event/BaseComponentEvents.h"

Component::Component(Handle type, Handle owner, MessagingSystem* messaging)
  : mOwner(owner)
  , mType(type)
  , mMessaging(messaging) {
  if(mMessaging)
    mMessaging->fireEvent(AddComponentEvent(mOwner, getHandle()));
}

Component::~Component() {
  if(mMessaging)
    mMessaging->fireEvent(RemoveComponentEvent(mOwner, getHandle()));
}