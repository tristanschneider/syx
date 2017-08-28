#pragma once
#include "SyxTransform.h"
#include "SyxRigidbody.h"
#include "SyxCollider.h"

namespace Syx {
  namespace PhysicsObjectFlags {
    enum {
      Disabled = 1 << 0,
      Asleep = 1 << 1
    };
  }

  SAlign class PhysicsObject {
  public:
    DeclareHandleMapNode(PhysicsObject);

    PhysicsObject();
    PhysicsObject(Handle myHandle);
    PhysicsObject(const PhysicsObject& obj) { *this = obj; }

    bool operator<(Handle rhs) { return mMyHandle < rhs; }
    bool operator==(Handle rhs) { return mMyHandle == rhs; }
    PhysicsObject& operator=(const PhysicsObject& rhs);
    Handle getHandle() { return mMyHandle; }

    //Always use these so the location of them can easily be swapped out for cache coherency where it's needed
    Collider* getCollider() { return mCollider.getFlag(ColliderFlags::Disabled) ? nullptr : &mCollider; }
    Rigidbody* getRigidbody() { return mRigidbody.getFlag(RigidbodyFlags::Disabled) ? nullptr : &mRigidbody; }
    Transform& getTransform() { return mTransform; }

    bool isStatic();

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

  private:
    SAlign Transform mTransform;
    SAlign Collider mCollider;
    SAlign Rigidbody mRigidbody;
    int mFlags;
    Handle mMyHandle;
    std::unordered_set<Handle> mConstraints;
  };
}