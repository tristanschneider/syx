#include "Precompile.h"
#include "component/Physics.h"

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

DEFINE_COMPONENT(Physics) {
}

void Physics::setData(const PhysicsData& data, bool fireEvent) {
  mData = data;
}

void Physics::setCollider(Handle model, Handle material) {
  mData.mHasCollider = true;
  mData.mModel = model;
  mData.mMaterial = material;
}

void Physics::setRigidbody(const Syx::Vec3& linVel, const Syx::Vec3& angVel) {
  mData.mHasRigidbody = true;
  mData.mLinVel = linVel;
  mData.mAngVel = angVel;
}

void Physics::setPhysToModel(const Syx::Mat4& physToModel) {
  mData.mPhysToModel = physToModel;
}

void Physics::setLinVel(const Syx::Vec3& linVel) {
  mData.mLinVel = linVel;
}

void Physics::setAngVel(const Syx::Vec3& angVel) {
  mData.mAngVel = angVel;
}
