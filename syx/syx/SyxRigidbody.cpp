#include "Precompile.h"
#include "SyxRigidbody.h"
#include "SyxPhysicsObject.h"
#include "SyxModel.h"
#include "SyxPhysicsSystem.h"

namespace Syx {
  const float MASS_EPSILON = 0.000001f;
#ifdef SENABLED
  void Rigidbody::sCalculateMass(void) {

  }
#endif

  Rigidbody::Rigidbody(const Rigidbody& rb, PhysicsObject* owner) {
    _reassign(rb, owner);
  }

  void Rigidbody::updateInertia(void) {
    Mat3 rot = mOwner->getTransform().mRot.toMatrix();
    mInvInertia = rot.scaled(mLocalInertia) * rot.transposed();
  }

  void Rigidbody::calculateMass(void) {
    Collider* collider = mOwner->getCollider();
    if(!collider) {
      //Default to identity values so colliderless objects with rigidbodies can still move with velocity
      mInvInertia = Vec3::Identity;
      mInvMass = 1.0f;
      return;
    }

    Vec3 scale = mOwner->getTransform().mScale;
    MassInfo info = collider->getModel()._computeMasses(scale);
    mInvMass = info.mMass;
    mLocalInertia = info.mInertia;

    if(getFlag(RigidbodyFlags::LockAngX)) {
      mLocalInertia.x = 0;
    }
    if(getFlag(RigidbodyFlags::LockAngY)) {
      mLocalInertia.y = 0;
    }
    if(getFlag(RigidbodyFlags::LockAngZ)) {
      mLocalInertia.z = 0;
    }

    float density = collider->getModelInstance().getMaterial().mDensity;
    mInvMass *= density;
    mLocalInertia *= density;

    // Use no epsilon. Masses can get tiny and still be okay
    mInvMass = safeDivide(1.0f, mInvMass, MASS_EPSILON);
    for(int i = 0; i < 3; ++i)
      mLocalInertia[i] = safeDivide(1.0f, mLocalInertia[i], MASS_EPSILON);

    updateInertia();
  }

  void Rigidbody::integratePosition(float dt) {
    Transform& t = mOwner->getTransform();
    t.mPos += mLinVel*dt;

    Quat quatVel(mAngVel, 0.0f);
    Quat spin = 0.5f*quatVel*t.mRot;
    t.mRot += spin*dt;
    t.mRot.normalize();

    updateInertia();
  }

  void Rigidbody::integrateVelocity(float dt) {
    if(mInvMass < SYX_EPSILON)
      return;

    //Other accelerations would go here, but there's only gravity now
    mLinVel += getGravity()*dt;
  }

  PhysicsObject* Rigidbody::getOwner(void) {
    return mOwner;
  }

  void Rigidbody::setFlag(int flag, bool value) {
    if(setBits(mFlags, flag, value)) {
      if(flag & (RigidbodyFlags::LockAngX | RigidbodyFlags::LockAngY | RigidbodyFlags::LockAngZ)) {
          calculateMass();
      }
    }
  }

  Vec3 Rigidbody::getGravity() {
    return Vec3(0.0f, -10.0f, 0.0f);
  }

  Vec3 Rigidbody::getUnintegratedLinearVelocity() {
    return mLinVel - getGravity()*PhysicsSystem::sSimRate;
  }

  Vec3 Rigidbody::getUnintegratedAngularVelocity() {
    //No angular acceleration, so it's just this
    return mAngVel;
  }

  void Rigidbody::applyImpulse(const Vec3& linear, const Vec3& angular, Space* space) {
    mLinVel += mInvMass * linear;
    mAngVel += mInvInertia * angular;
    if(mOwner && space) {
      space->wakeObject(*mOwner);
    }
  }

  void Rigidbody::applyImpulseAtPoint(const Vec3& impulse, const Vec3& point, Space& space) {
    const Vec3 toPoint = point - getCenterOfMass();
    applyImpulse(impulse, toPoint.cross(impulse), &space);
  }

  const Vec3& Rigidbody::getCenterOfMass() const {
    return mOwner->getTransform().mPos;
  }

  void Rigidbody::_reassign(const Rigidbody& rb, PhysicsObject* newOwner) {
    mLocalInertia = rb.mLocalInertia;
    mInvInertia = rb.mInvInertia;
    mInvMass = rb.mInvMass;
    mFlags = rb.mFlags;
    mOwner = newOwner;
  }
}