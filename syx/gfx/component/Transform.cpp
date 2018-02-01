#include "Precompile.h"
#include "Transform.h"
#include "MessageQueueProvider.h"
#include "event/TransformEvent.h"

Transform::Transform(Handle owner, MessageQueueProvider* messaging)
  : Component(static_cast<Handle>(ComponentType::Transform), owner, messaging)
  , mMat(Syx::Mat4::transform(Syx::Quat::Identity, Syx::Vec3::Zero)) {
}

void Transform::set(const Syx::Mat4& m, bool fireEvent) {
  mMat = m;
  if(fireEvent)
    _fireEvent();
}

const Syx::Mat4& Transform::get() {
  return mMat;
}

void Transform::_fireEvent() {
  mMessaging->getMessageQueue().get().push(TransformEvent(mOwner, mMat));
}
