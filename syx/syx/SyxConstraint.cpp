#include "Precompile.h"
#include "SyxConstraint.h"
#include "SyxPhysicsObject.h"
#include "SyxPhysicsSystem.h"

namespace Syx {
  const float LocalConstraint::sVelBaumgarteTerm = 0.1f/PhysicsSystem::sSimRate;
  const float LocalConstraint::sMaxVelCorrection = 10.0f;

  float Constraint::sDT;

  LocalObject::LocalObject()
    : mOwner(nullptr) {}

  LocalObject::LocalObject(PhysicsObject& owner)
    : mOwner(&owner)
    , mPos(owner.GetTransform().mPos)
    , mRot(owner.GetTransform().mRot) {
    if(Rigidbody* rb = owner.GetRigidbody()) {
      mLinVel = rb->mLinVel;
      mAngVel = rb->mAngVel;
      mInertia = rb->GetInertia();
      mInvMass = rb->GetMass();
    }
    else {
      mLinVel = mAngVel = Vec3::Zero;
      mInvMass = 0.0f;
      mInertia = Mat3::Zero;
    }
  }

  Vec3 LocalObject::ModelToWorldPoint(const Vec3& p) const {
    return (mRot * p) + mPos;
  }

  Vec3 LocalObject::ModelToWorldVec(const Vec3& v) const {
    return mRot * v;
  }

  Vec3 LocalObject::WorldToModelPoint(const Vec3& p) const {
    return mRot.Inversed() * (p - mPos);
  }

  Vec3 LocalObject::WorldToModelVec(const Vec3& v) const {
    return mRot.Inversed() * v;
  }

  void ConstraintObjBlock::Set(const LocalObject& obj) {
    mPos = obj.mPos;
    mRot = obj.mRot;
    LoadVelocity(obj);
  }

  void ConstraintObjBlock::LoadVelocity(const LocalObject& obj) {
    mLinVel = obj.mLinVel;
    mAngVel = obj.mAngVel;
  }

  void ConstraintObjBlock::StoreVelocity(LocalObject& obj) const {
    obj.mLinVel = mLinVel;
    obj.mAngVel = mAngVel;
  }
}