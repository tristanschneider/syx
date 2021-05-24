#pragma once
#include "SyxTransform.h"
#include "SyxRigidbody.h"
#include "SyxCollider.h"
#include "SyxResourceHandle.h"

namespace Syx {
  enum class PhysicsObjectFlags : uint8_t {
    Disabled = 0,
    Asleep,
    Count,
  };

  struct CollisionEventSubscription;

  SAlign class PhysicsObject : public EnableDeferredDeletion<PhysicsObject> {
  public:
    using HandleT = DeferredDeleteResourceHandle<PhysicsObject>;

    PhysicsObject(Handle myHandle, HandleT handle);
    PhysicsObject(PhysicsObject&& rhs);

    PhysicsObject& operator=(PhysicsObject&& rhs);

    bool operator<(Handle rhs) { return mMyHandle < rhs; }
    bool operator==(Handle rhs) { return mMyHandle == rhs; }
    PhysicsObject& operator=(const PhysicsObject& rhs);
    Handle getHandle() const { return mMyHandle; }

    //Always use these so the location of them can easily be swapped out for cache coherency where it's needed
    Collider* getCollider() { return mCollider.getFlag(ColliderFlags::Disabled) ? nullptr : &mCollider; }
    Rigidbody* getRigidbody() { return mRigidbody.getFlag(RigidbodyFlags::Disabled) ? nullptr : &mRigidbody; }
    Transform& getTransform() { return mTransform; }
    const Transform& getTransform() const { return mTransform; }

    void setTransform(const Transform& transform);

    bool isStatic();

    //Sets the raw sleep state, awaken is needed if wanting to wake up a sleeping object
    void setAsleep(bool asleep);
    bool getAsleep();
    //If this is inactive enough that it could go to sleep if everything else was in place
    bool isInactive();

    void setRigidbodyEnabled(bool enabled);
    void setColliderEnabled(bool enabled);

    void updateModelInst();

    void drawModel();

    void removeConstraint(Handle handle);
    void addConstraint(Handle handle);
    const std::unordered_set<Handle>& getConstraints();
    void clearConstraints();

    bool shouldIntegrate();

    std::shared_ptr<CollisionEventSubscription> addCollisionEventSubscription();
    bool emitsCollisionEvents() const;

  private:
    SAlign Transform mTransform;
    SAlign Collider mCollider;
    SAlign Rigidbody mRigidbody;
    std::bitset<static_cast<size_t>(PhysicsObjectFlags::Count)> mFlags;
    Handle mMyHandle;
    std::unordered_set<Handle> mConstraints;
    //As long as someone is holding on to one of these the object will emit collision events
    std::weak_ptr<CollisionEventSubscription> mCollisionEventToken;
  };
  //constexpr size_t asdf = sizeof(PhysicsObject);
  struct PhysicsObjectInternalHandle : public DeferredDeleteResourceHandle<PhysicsObject> {
    using DeferredDeleteResourceHandle::DeferredDeleteResourceHandle;
    PhysicsObjectInternalHandle(DeferredDeleteResourceHandle<PhysicsObject> obj)
      : DeferredDeleteResourceHandle(std::move(obj)) {
    }
    const PhysicsObject* operator->() const { return &_get(); }
    PhysicsObject* operator->() { return &_get(); }
    PhysicsObject& operator*() { return _get(); }
    const PhysicsObject& operator*() const { return _get(); }
  };
}