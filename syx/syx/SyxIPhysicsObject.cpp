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
      mObj->applyImpulse(linear, angular, *mSpace);
    }

    void applyImpulseAtPoint(const Vec3& impulse, const Vec3& point) override {
      mObj->applyImpulseAtPoint(impulse, point, *mSpace);
    }

  private:
    Rigidbody* mObj = nullptr;
    Space* mSpace = nullptr;
  };

  struct PhysicsObjectReference : public IPhysicsObject {
    PhysicsObjectReference(PhysicsObject& obj, std::weak_ptr<bool> existenceTracker, Space& space)
      : mObj(obj)
      , mExistence(std::move(existenceTracker))
      , mSpace(space) {
    }

    bool isValid() const override {
      return !mExistence.expired();
    }

    Handle getHandle() const override {
      return mObj.getHandle();
    }

    const Transform& getTransform() const override {
      return mObj.getTransform();
    }

    void setTransform(const Transform& transform) override {
      mObj.setTransform(transform);
    }

    void setRigidbodyEnabled(bool enabled) override {
      mObj.setRigidbodyEnabled(enabled);
    }

    void setColliderEnabled(bool enabled) override {
      mObj.setColliderEnabled(enabled);
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
      return mObj.addCollisionEventSubscription();
    }

    void _updateColliderStorage() {
      Collider* collider = mObj.getCollider();
      mColliderStorage = collider ? std::make_optional(ColliderWrapper(*collider, mSpace)) : std::nullopt;
    }

    void _updateRigidbodyStorage() {
      Rigidbody* rigidbody = mObj.getRigidbody();
      mRigidbodyStorage = rigidbody ? std::make_optional(RigidbodyWrapper(*rigidbody, mSpace)) : std::nullopt;
    }

    PhysicsObject& mObj;
    std::weak_ptr<bool> mExistence;
    std::optional<ColliderWrapper> mColliderStorage;
    std::optional<RigidbodyWrapper> mRigidbodyStorage;
    Space& mSpace;
  };

  std::shared_ptr<IPhysicsObject> createPhysicsObjectRef(PhysicsObject& obj, std::weak_ptr<bool> existenceTracker, Space& space) {
    return std::make_shared<PhysicsObjectReference>(obj, existenceTracker, space);
  }
}