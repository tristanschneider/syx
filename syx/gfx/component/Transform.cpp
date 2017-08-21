#include "Precompile.h"
#include "Transform.h"
#include "system/MessagingSystem.h"

Transform::Transform(Handle owner, MessagingSystem* messaging)
  : Component(owner, messaging)
  , mMat(Syx::Mat4::transform(Syx::Quat::Identity, Syx::Vec3::Zero)) {
}

void Transform::set(const Syx::Mat4& m) {
  mMat = m;
  fireEvent();
}

const Syx::Mat4& Transform::get() {
  return mMat;
}

void Transform::fireEvent() {
  mMessaging->fireTransformEvent(TransformEvent(mOwner, mMat));
}
