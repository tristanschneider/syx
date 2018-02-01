#include "Precompile.h"
#include "component/Component.h"
#include "MessageQueueProvider.h"
#include "event/BaseComponentEvents.h"

Component::Component(Handle type, Handle owner, MessageQueueProvider* messaging)
  : mOwner(owner)
  , mType(type)
  , mMessaging(messaging) {
  if(mMessaging)
    mMessaging->getMessageQueue().get().push(AddComponentEvent(mOwner, getHandle()));
}

Component::~Component() {
  if(mMessaging)
    mMessaging->getMessageQueue().get().push(RemoveComponentEvent(mOwner, getHandle()));
}