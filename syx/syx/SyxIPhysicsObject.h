#pragma once

namespace Syx {
  struct CollisionEventSubscription;
  class PhysicsObject;
  class Space;
  struct Transform;

  struct ICollider {
    virtual ~ICollider() = default;
  };

  struct IRigidbody {
    virtual ~IRigidbody() = default;

    virtual const Vec3& getLinearVelocity() const = 0;
    virtual void setLinearVelocity(const Vec3& velocity) = 0;
    virtual const Vec3& getAngularVelocity() const = 0;
    virtual void setAngularVelocity(const Vec3& velocity) = 0;

    virtual const Mat3& getInverseInertia() const = 0;
    virtual const float getInverseMass() const = 0;

    virtual void applyImpulse(const Vec3& linear, const Vec3& angular) = 0;
    virtual void applyImpulseAtPoint(const Vec3& impulse, const Vec3& point) = 0;
  };

  struct IPhysicsObject {
    virtual ~IPhysicsObject() = default;

    //Not thread-safe but neither is the rest of this class
    virtual bool isValid() const = 0;
    virtual Handle getHandle() const = 0;

    virtual const Transform& getTransform() const = 0;
    virtual void setTransform(const Transform& transform) = 0;

    virtual void setRigidbodyEnabled(bool enabled) = 0;
    virtual void setColliderEnabled(bool enabled) = 0;

    virtual ICollider* tryGetCollider() = 0;
    virtual IRigidbody* tryGetRigidbody() = 0;

    virtual std::shared_ptr<CollisionEventSubscription> addCollisionEventSubscription() = 0;
  };

  //Existence tracker will do for now. Better would be an invalidation mechanism
  std::shared_ptr<IPhysicsObject> createPhysicsObjectRef(PhysicsObject& obj, std::weak_ptr<bool> existenceTracker, Space& space);
}