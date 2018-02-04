#include "Precompile.h"
#include "component/Physics.h"

#include "event/EventBuffer.h"
#include "MessageQueueProvider.h"

DEFINE_EVENT(PhysicsCompUpdateEvent, const PhysicsData& data, Handle owner)
  , mData(data)
  , mOwner(owner) {
}

PhysicsData::PhysicsData()
  : mHasRigidbody(false)
  , mHasCollider(false)
  , mModel(InvalidHandle)
  , mMaterial(InvalidHandle)
  , mPhysToModel(Syx::Mat4::identity())
  , mLinVel(Syx::Vec3::Zero)
  , mAngVel(Syx::Vec3::Zero) {
}

Physics::Physics(Handle owner, MessageQueueProvider& messaging)
  : Component(static_cast<Handle>(ComponentType::Physics), owner, &messaging) {
}

void Physics::setData(const PhysicsData& data, bool fireEvent) {
  mData = data;
  if(fireEvent)
    fireUpdateEvent();
}

void Physics::setCollider(Handle model, Handle material) {
  mData.mHasCollider = true;
  mData.mModel = model;
  mData.mMaterial = material;
  fireUpdateEvent();
}

void Physics::setRigidbody(const Syx::Vec3& linVel, const Syx::Vec3& angVel) {
  mData.mHasRigidbody = true;
  mData.mLinVel = linVel;
  mData.mAngVel = angVel;
  fireUpdateEvent();
}

void Physics::setPhysToModel(const Syx::Mat4& physToModel) {
  mData.mPhysToModel = physToModel;
  fireUpdateEvent();
}

void Physics::setLinVel(const Syx::Vec3& linVel) {
  mData.mLinVel = linVel;
  fireUpdateEvent();
}

void Physics::setAngVel(const Syx::Vec3& angVel) {
  mData.mAngVel = angVel;
  fireUpdateEvent();
}

void Physics::fireUpdateEvent() {
  mMessaging->getMessageQueue().get().push(PhysicsCompUpdateEvent(mData, mOwner));
}
