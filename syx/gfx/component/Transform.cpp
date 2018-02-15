#include "Precompile.h"
#include "Transform.h"
#include "provider/MessageQueueProvider.h"
#include "event/EventBuffer.h"
#include "event/TransformEvent.h"

DEFINE_COMPONENT(Transform)
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
