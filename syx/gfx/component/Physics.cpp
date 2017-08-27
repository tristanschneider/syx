#include "Precompile.h"
#include "component/Physics.h"
#include "system/MessagingSystem.h"

PhysicsData::PhysicsData()
  : mHasRigidbody(false)
  , mHasCollider(false)
  , mModel(InvalidHandle)
  , mMaterial(InvalidHandle)
  , mPhysToModel(Syx::Mat4::identity())
  , mLinVel(Syx::Vec3::Zero)
  , mAngVel(Syx::Vec3::Zero) {
}

PhysicsCompUpdateEvent::PhysicsCompUpdateEvent(const PhysicsData& data, Handle owner)
  : Event(EventFlag::Physics)
  , mData(data)
  , mOwner(owner) {
}

std::unique_ptr<Event> PhysicsCompUpdateEvent::clone() const {
  return std::make_unique<PhysicsCompUpdateEvent>(mData, mOwner);
}

Physics::Physics(Handle owner, MessagingSystem& messaging)
  : Component(owner, &messaging) {
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

void Physics::fireUpdateEvent() {
  mMessaging->fireEvent(std::make_unique<PhysicsCompUpdateEvent>(mData, mOwner));
}

