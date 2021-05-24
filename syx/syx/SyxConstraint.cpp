#include "Precompile.h"
#include "SyxConstraint.h"
#include "SyxPhysicsObject.h"
#include "SyxPhysicsSystem.h"

namespace Syx {
  const float LocalConstraint::sVelBaumgarteTerm = 0.1f/PhysicsSystem::sSimRate;
  const float LocalConstraint::sMaxVelCorrection = 10.0f;

  float Constraint::sDT;

  Constraint::Constraint(ConstraintType type, PhysicsObject* a, PhysicsObject* b, Handle handle)
    : mA(a ? PhysicsObjectHandle(a->duplicateHandle().value_or(PhysicsObject::HandleT())) : PhysicsObjectHandle())
    , mB(b ? PhysicsObjectHandle(b->duplicateHandle().value_or(PhysicsObject::HandleT())) : PhysicsObjectHandle())
    , mShouldRemove(false)
    , mHandle(handle)
    , mType(type)
    , mBlacklistCollision(true) {
  }

  PhysicsObject* Constraint::getObjA() {
    PhysicsObjectInternalHandle ptr(mA.lock());
    return ptr ? &*ptr : nullptr;
  }

  PhysicsObject* Constraint::getObjB() {
    PhysicsObjectInternalHandle ptr(mB.lock());
    return ptr ? &*ptr : nullptr;
  }

  LocalObject::LocalObject()
    : mOwner(nullptr) {}

  LocalObject::LocalObject(PhysicsObject& owner)
    : mOwner(&owner)
    , mPos(owner.getTransform().mPos)
    , mRot(owner.getTransform().mRot) {
    if(Rigidbody* rb = owner.getRigidbody()) {
      mLinVel = rb->mLinVel;
      mAngVel = rb->mAngVel;
      mInertia = rb->getInertia();
      mInvMass = rb->getMass();
    }
    else {
      mLinVel = mAngVel = Vec3::Zero;
      mInvMass = 0.0f;
      mInertia = Mat3::Zero;
    }
  }

  Vec3 LocalObject::modelToWorldPoint(const Vec3& p) const {
    return (mRot * p) + mPos;
  }

  Vec3 LocalObject::modelToWorldVec(const Vec3& v) const {
    return mRot * v;
  }

  Vec3 LocalObject::worldToModelPoint(const Vec3& p) const {
    return mRot.inversed() * (p - mPos);
  }

  Vec3 LocalObject::worldToModelVec(const Vec3& v) const {
    return mRot.inversed() * v;
  }

  void ConstraintObjBlock::set(const LocalObject& obj) {
    mPos = obj.mPos;
    mRot = obj.mRot;
    loadVelocity(obj);
  }

  void ConstraintObjBlock::loadVelocity(const LocalObject& obj) {
    mLinVel = obj.mLinVel;
    mAngVel = obj.mAngVel;
  }

  void ConstraintObjBlock::storeVelocity(LocalObject& obj) const {
    obj.mLinVel = mLinVel;
    obj.mAngVel = mAngVel;
  }
}