#include "Precompile.h"
#include "SyxPhysicsObject.h"

#include "SyxEvents.h"
#include "SyxModel.h"

namespace Syx {
  PhysicsObject::PhysicsObject(Handle myHandle, HandleT handle)
    : EnableDeferredDeletion(std::move(handle))
    , mRigidbody(this)
    , mCollider(this)
    , mFlags(0)
    , mMyHandle(myHandle) {
  }

  PhysicsObject::PhysicsObject(PhysicsObject&& rhs)
    : EnableDeferredDeletion(std::move(rhs))
    , mRigidbody(rhs.mRigidbody, this)
    , mCollider(rhs.mCollider, this)
    , mTransform(rhs.mTransform)
    , mFlags(rhs.mFlags)
    , mMyHandle(rhs.mMyHandle)
    , mConstraints(std::move(rhs.mConstraints))
    , mCollisionEventToken(std::move(rhs.mCollisionEventToken)) {
  }

  PhysicsObject& PhysicsObject::operator=(PhysicsObject&& rhs) {
    mRigidbody._reassign(rhs.mRigidbody, this);
    mCollider._reassign(rhs.mCollider, this);
    mTransform = rhs.mTransform;
    mFlags = rhs.mFlags;
    mMyHandle = rhs.mMyHandle;
    mConstraints = std::move(rhs.mConstraints);
    mCollisionEventToken = std::move(rhs.mCollisionEventToken);
    EnableDeferredDeletion::operator=(std::move(rhs));
    return *this;
  }

  void PhysicsObject::setRigidbodyEnabled(bool enabled) {
    mRigidbody.setFlag(RigidbodyFlags::Disabled, !enabled);
  }

  void PhysicsObject::setColliderEnabled(bool enabled) {
    mCollider.setFlag(ColliderFlags::Disabled, !enabled);
  }

  void PhysicsObject::drawModel() {
    DebugDrawer& d = DebugDrawer::get();
    Collider* collider = getCollider();
    if(!collider)
      return;

    int options = Interface::getOptions().mDebugFlags;

    if(options & SyxOptions::Debug::DrawModels)
      collider->getModel().draw(mTransform.getModelToWorld());

    d.setColor(0.0f, 1.0f, 1.0f);
    if(options & SyxOptions::Debug::DrawPersonalBBs)
      collider->getAABB().draw();
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

  void PhysicsObject::updateModelInst() {
    Collider* collider = getCollider();
    if(collider)
      collider->updateModelInst(mTransform);
  }

  void PhysicsObject::setTransform(const Transform& transform) {
    //TODO: probably recompute some stuff here
    mTransform = transform;
  }

  bool PhysicsObject::isStatic() {
    //Might want to include rigidbodies with infinite mass, but I'm not sure what the pros and cons are
    return getRigidbody() == nullptr;
  }

  void PhysicsObject::setAsleep(bool asleep) {
    mFlags.set(static_cast<size_t>(PhysicsObjectFlags::Asleep), asleep);
    //Need to inform broadphase that these nodes can be skipped...
  }

  bool PhysicsObject::getAsleep() {
    return mFlags.test(static_cast<size_t>(PhysicsObjectFlags::Asleep));
  }

  bool PhysicsObject::isInactive() {
    float linearThreshold2 = 0.001f;
    float angularThreshold2 = 0.00001f;

    Rigidbody* rb = getRigidbody();
    //If it doesn't have a rigidbody, it can't move, so is obviously inactive
    if(!rb)
      return true;
    //This is called after integration but before solving, so factor out energy added by integration, assuming it'll be solved away
    return rb->getUnintegratedLinearVelocity().length2() < linearThreshold2 && rb->getUnintegratedAngularVelocity().length2() < angularThreshold2;
  }

  void PhysicsObject::removeConstraint(Handle handle) {
    mConstraints.erase(handle);
  }

  void PhysicsObject::addConstraint(Handle handle) {
    mConstraints.insert(handle);
  }

  const std::unordered_set<Handle>& PhysicsObject::getConstraints() {
    return mConstraints;
  }

  void PhysicsObject::clearConstraints() {
    mConstraints.clear();
  }

  bool PhysicsObject::shouldIntegrate() {
    Rigidbody* rb = getRigidbody();
    return !getAsleep() && rb && rb->mInvMass > 0.0f;
  }

  std::shared_ptr<CollisionEventSubscription> PhysicsObject::addCollisionEventSubscription() {
    if(auto result = mCollisionEventToken.lock()) {
      return result;
    }
    auto result = std::make_shared<CollisionEventSubscription>();
    mCollisionEventToken = result;
    return result;
  }

  bool PhysicsObject::emitsCollisionEvents() const {
    return !mCollisionEventToken.expired();
  }
}