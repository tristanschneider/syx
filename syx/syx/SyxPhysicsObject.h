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
    Handle GetHandle() { return mMyHandle; }

    //Always use these so the location of them can easily be swapped out for cache coherency where it's needed
    Collider* GetCollider() { return mCollider.GetFlag(ColliderFlags::Disabled) ? nullptr : &mCollider; }
    Rigidbody* GetRigidbody() { return mRigidbody.GetFlag(RigidbodyFlags::Disabled) ? nullptr : &mRigidbody; }
    Transform& GetTransform() { return mTransform; }

    bool IsStatic();

    void SetAsleep(bool asleep);
    bool GetAsleep();
    //If this is inactive enough that it could go to sleep if everything else was in place
    bool IsInactive();

    void SetRigidbodyEnabled(bool enabled);
    void SetColliderEnabled(bool enabled);

    void UpdateModelInst();

    void DrawModel();

    void RemoveConstraint(Handle handle);
    void AddConstraint(Handle handle);
    const std::unordered_set<Handle>& GetConstraints();
    void ClearConstraints();

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