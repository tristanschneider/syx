#include "Precompile.h"
#include "Transform.h"
#include "system/MessagingSystem.h"
#include "event/TransformEvent.h"

Transform::Transform(Handle owner, MessagingSystem* messaging)
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
  mMessaging->fireEvent(TransformEvent(mOwner, mMat));
}
