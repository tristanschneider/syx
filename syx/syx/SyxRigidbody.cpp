#include "Precompile.h"
#include "SyxRigidbody.h"
#include "SyxPhysicsObject.h"
#include "SyxModel.h"
#include "SyxPhysicsSystem.h"

namespace Syx {
#ifdef SENABLED
  void Rigidbody::SCalculateMass(void) {

  }
#endif

  void Rigidbody::UpdateInertia(void) {
    Mat3 rot = mOwner->GetTransform().mRot.ToMatrix();
    mInvInertia = rot.Scaled(mLocalInertia) * rot.Transposed();
  }

  void Rigidbody::CalculateMass(void) {
    Collider* collider = mOwner->GetCollider();
    if(!collider) {
      //Default to identity values so colliderless objects with rigidbodies can still move with velocity
      mInvInertia = Vec3::Identity;
      mInvMass = 1.0f;
      return;
    }

    Vec3 scale = mOwner->GetTransform().mScale;
    MassInfo info = collider->GetModel().ComputeMasses(scale);
    mInvMass = info.mMass;
    mLocalInertia = info.mInertia;

    float density = collider->GetModelInstance().GetMaterial().m_density;
    mInvMass *= density;
    mLocalInertia *= density;

    // Use no epsilon. Masses can get tiny and still be okay
    mInvMass = SafeDivide(1.0f, mInvMass, 0.0f);
    for(int i = 0; i < 3; ++i)
      mLocalInertia[i] = SafeDivide(1.0f, mLocalInertia[i], 0.0f);

    UpdateInertia();
  }

  void Rigidbody::IntegratePosition(float dt) {
    if(mInvMass < SYX_EPSILON)
      return;

    Transform& t = mOwner->GetTransform();
    t.mPos += mLinVel*dt;

    Quat quatVel(mAngVel, 0.0f);
    Quat spin = 0.5f*quatVel*t.mRot;
    t.mRot += spin*dt;
    t.mRot.Normalize();

    UpdateInertia();
  }

  void Rigidbody::IntegrateVelocity(float dt) {
    if(mInvMass < SYX_EPSILON)
      return;

    //Other accelerations would go here, but there's only gravity now
    mLinVel += GetGravity()*dt;
  }

  PhysicsObject* Rigidbody::GetOwner(void) {
    return mOwner;
  }

  Vec3 Rigidbody::GetGravity() {
    return Vec3(0.0f, -10.0f, 0.0f);
  }

  Vec3 Rigidbody::GetUnintegratedLinearVelocity() {
    return mLinVel - GetGravity()*PhysicsSystem::sSimRate;
  }

  Vec3 Rigidbody::GetUnintegratedAngularVelocity() {
    //No angular acceleration, so it's just this
    return mAngVel;
  }
}