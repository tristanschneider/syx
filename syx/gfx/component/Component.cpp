#include "Precompile.h"
#include "component/Component.h"

#include "event/BaseComponentEvents.h"
#include "event/EventBuffer.h"
#include "provider/MessageQueueProvider.h"

Component::Component(Handle type, Handle owner, MessageQueueProvider* messaging)
  : mOwner(owner)
  , mType(type)
  , mMessaging(messaging) {
  if(mMessaging)
    mMessaging->getMessageQueue().get().push(AddComponentEvent(mOwner, mType));
}

Component::~Component() {
  if(mMessaging)
    mMessaging->getMessageQueue().get().push(RemoveComponentEvent(mOwner, mType));
}