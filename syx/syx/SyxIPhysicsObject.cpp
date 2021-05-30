#include "Precompile.h"
#include "SyxIPhysicsObject.h"

#include <optional>
#include "SyxPhysicsObject.h"
#include "SyxSpace.h"

namespace Syx {
  struct ColliderWrapper : public ICollider {
    ColliderWrapper(Collider& obj, Space& space)
      : mObj(&obj)
      , mSpace(&space) {
    }

    virtual void setMaterial(const IMaterialHandle& handle) override {
      mObj->setMaterial(handle);
    }

    virtual void setModel(std::shared_ptr<const Model> model) override {
      mObj->setModel(std::move(model), *mSpace);
    }

  private:
    Collider* mObj = nullptr;
    Space* mSpace = nullptr;
  };

  struct RigidbodyWrapper : public IRigidbody {
    RigidbodyWrapper(Rigidbody& obj, Space& space)
      : mObj(&obj)
      , mSpace(&space) {
    }

    void setAxisRotationLocked(Axis axis, bool locked) override {
      switch(axis) {
        case Axis::X: mObj->setFlag(RigidbodyFlags::LockAngX, locked); break;
        case Axis::Y: mObj->setFlag(RigidbodyFlags::LockAngY, locked); break;
        case Axis::Z: mObj->setFlag(RigidbodyFlags::LockAngZ, locked); break;
      }
    }

    const Vec3& getLinearVelocity() const override {
      return mObj->mLinVel;
    }

    void setLinearVelocity(const Vec3& velocity) override {
      mObj->mLinVel = velocity;
    }

    const Vec3& getAngularVelocity() const override {
      return mObj->mAngVel;
    }

    void setAngularVelocity(const Vec3& velocity) override {
      mObj->mAngVel = velocity;
    }

    const Mat3& getInverseInertia() const override {
      return mObj->getInertia();
    }

    const float getInverseMass() const override {
      return mObj->getMass();
    }

    void applyImpulse(const Vec3& linear, const Vec3& angular) override {
      mObj->applyImpulse(linear, angular, mSpace);
    }

    void applyImpulseAtPoint(const Vec3& impulse, const Vec3& point) override {
      mObj->applyImpulseAtPoint(impulse, point, *mSpace);
    }

  private:
    Rigidbody* mObj = nullptr;
    Space* mSpace = nullptr;
  };

  struct PhysicsObjectReference : public IPhysicsObject {
    PhysicsObjectReference(PhysicsObjectInternalHandle handle, Space& space)
      : mHandle(std::move(handle))
      , mSpace(space) {
    }

    bool isValid() const override {
      return mHandle;
    }

    Handle getHandle() const override {
      return mHandle->getHandle();
    }

    const Transform& getTransform() const override {
      return mHandle->getTransform();
    }

    void setTransform(const Transform& transform) override {
      mHandle->setTransform(transform);
    }

    void setRigidbodyEnabled(bool enabled) override {
      mHandle->setRigidbodyEnabled(enabled);
    }

    void setColliderEnabled(bool enabled) override {
      mHandle->setColliderEnabled(enabled);
    }

    ICollider* tryGetCollider() override {
      _updateColliderStorage();
      return mColliderStorage ? &*mColliderStorage : nullptr;
    }

    IRigidbody* tryGetRigidbody() override {
      _updateRigidbodyStorage();
      return mRigidbodyStorage ? &*mRigidbodyStorage : nullptr;
    }

    std::shared_ptr<CollisionEventSubscription> addCollisionEventSubscription() override {
      return mHandle->addCollisionEventSubscription();
    }

    void _updateColliderStorage() {
      Collider* collider = mHandle->getCollider();
      mColliderStorage = collider ? std::make_optional(ColliderWrapper(*collider, mSpace)) : std::nullopt;
    }

    void _updateRigidbodyStorage() {
      Rigidbody* rigidbody = mHandle->getRigidbody();
      mRigidbodyStorage = rigidbody ? std::make_optional(RigidbodyWrapper(*rigidbody, mSpace)) : std::nullopt;
    }

    PhysicsObjectInternalHandle mHandle;
    std::optional<ColliderWrapper> mColliderStorage;
    std::optional<RigidbodyWrapper> mRigidbodyStorage;
    Space& mSpace;
  };

  std::shared_ptr<IPhysicsObject> createPhysicsObjectRef(PhysicsObjectInternalHandle handle, Space& space) {
    return std::make_shared<PhysicsObjectReference>(std::move(handle), space);
  }
}