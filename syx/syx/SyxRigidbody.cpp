#include "Precompile.h"
#include "SyxRigidbody.h"
#include "SyxPhysicsObject.h"
#include "SyxModel.h"
#include "SyxPhysicsSystem.h"

namespace Syx {
#ifdef SENABLED
  void Rigidbody::sCalculateMass(void) {

  }
#endif

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

    float density = collider->getModelInstance().getMaterial().mDensity;
    mInvMass *= density;
    mLocalInertia *= density;

    // Use no epsilon. Masses can get tiny and still be okay
    mInvMass = safeDivide(1.0f, mInvMass, 0.0f);
    for(int i = 0; i < 3; ++i)
      mLocalInertia[i] = safeDivide(1.0f, mLocalInertia[i], 0.0f);

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
}