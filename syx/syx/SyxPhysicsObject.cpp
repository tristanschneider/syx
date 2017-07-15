#include "Precompile.h"
#include "SyxPhysicsObject.h"
#include "SyxModel.h"

namespace Syx {
  PhysicsObject::PhysicsObject()
    : mRigidbody(this)
    , mCollider(this)
    , mFlags(0) {
  }

  PhysicsObject::PhysicsObject(Handle myHandle)
    : mRigidbody(this)
    , mCollider(this)
    , mFlags(0)
    , mMyHandle(myHandle) {
  }

  void PhysicsObject::SetRigidbodyEnabled(bool enabled) {
    mRigidbody.SetFlag(RigidbodyFlags::Disabled, !enabled);
  }

  void PhysicsObject::SetColliderEnabled(bool enabled) {
    mCollider.SetFlag(ColliderFlags::Disabled, !enabled);
  }

  void PhysicsObject::DrawModel() {
    DebugDrawer& d = DebugDrawer::Get();
    Collider* collider = GetCollider();
    if(!collider)
      return;

    int options = Interface::GetOptions().mDebugFlags;

    if(options & SyxOptions::Debug::DrawModels)
      collider->GetModel().Draw(mTransform.GetModelToWorld());

    d.SetColor(0.0f, 1.0f, 1.0f);
    if(options & SyxOptions::Debug::DrawPersonalBBs)
      collider->GetAABB().Draw();
  }

  PhysicsObject& PhysicsObject::operator=(const PhysicsObject& rhs) {
    mCollider.mFlags = rhs.mCollider.mFlags;
    mCollider.mModelInst = rhs.mCollider.mModelInst;
    mCollider.mOwner = this;
    mCollider.mBroadHandle = rhs.mCollider.mBroadHandle;

    mRigidbody.mAngVel = rhs.mRigidbody.mAngVel;
    mRigidbody.mFlags = rhs.mRigidbody.mFlags;
    mRigidbody.mInvInertia = rhs.mRigidbody.mInvInertia;
    mRigidbody.mInvMass = rhs.mRigidbody.mInvMass;
    mRigidbody.mLinVel = rhs.mRigidbody.mLinVel;
    mRigidbody.mLocalInertia = rhs.mRigidbody.mLocalInertia;
    mRigidbody.mOwner = this;

    mFlags = rhs.mFlags;
    mMyHandle = rhs.mMyHandle;
    mTransform = rhs.mTransform;

    return *this;
  }

  void PhysicsObject::UpdateModelInst() {
    Collider* collider = GetCollider();
    if(collider)
      collider->UpdateModelInst(mTransform);
  }

  bool PhysicsObject::IsStatic() {
    //Might want to include rigidbodies with infinite mass, but I'm not sure what the pros and cons are
    return GetRigidbody() == nullptr;
  }

  void PhysicsObject::SetAsleep(bool asleep) {
    SetBits(mFlags, static_cast<int>(PhysicsObjectFlags::Asleep), asleep);
    //Need to inform broadphase that these nodes can be skipped...
  }

  bool PhysicsObject::GetAsleep() {
    return (mFlags & PhysicsObjectFlags::Asleep) != 0;
  }

  bool PhysicsObject::IsInactive() {
    float linearThreshold2 = 0.001f;
    float angularThreshold2 = 0.00001f;

    Rigidbody* rb = GetRigidbody();
    //If it doesn't have a rigidbody, it can't move, so is obviously inactive
    if(!rb)
      return true;
    //This is called after integration but before solving, so factor out energy added by integration, assuming it'll be solved away
    return rb->GetUnintegratedLinearVelocity().Length2() < linearThreshold2 && rb->GetUnintegratedAngularVelocity().Length2() < angularThreshold2;
  }

  void PhysicsObject::RemoveConstraint(Handle handle) {
    mConstraints.erase(handle);
  }

  void PhysicsObject::AddConstraint(Handle handle) {
    mConstraints.insert(handle);
  }

  const std::unordered_set<Handle>& PhysicsObject::GetConstraints() {
    return mConstraints;
  }

  void PhysicsObject::ClearConstraints() {
    mConstraints.clear();
  }
}