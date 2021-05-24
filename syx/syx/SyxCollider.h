#pragma once
#include "SyxModelInstance.h"

namespace Syx {
  class PhysicsObject;
  class Space;

  namespace ColliderFlags {
    enum {
      Ghost = 1 << 0,
      Disabled = 1 << 1
    };
  }

  SAlign class Collider {
  public:
    friend class PhysicsObject;

    Collider(PhysicsObject* owner = nullptr);
    Collider(const Collider& collider, PhysicsObject* owner);
    Collider& operator=(const Collider&) = delete;

    void initialize(Space& space);
    void uninitialize(Space& space);

    PhysicsObject* getOwner();

    void setFlag(int flag, bool value);
    bool getFlag(int flag);
    Vec3 getSupport(const Vec3& dir);
    SFloats sGetSupport(SFloats dir);

    void updateModelInst(const Transform& parentTransform);

    void setModel(std::shared_ptr<const Model> model);
    void setModel(std::shared_ptr<const Model> model, Space& space);
    void setMaterial(const IMaterialHandle& material);

    int getModelType();

    const Model& getModel() { return mModelInst.getModel(); }
    ModelInstance& getModelInstance() { return mModelInst; }
    const AABB& getAABB() { return mModelInst.getAABB(); }

    Handle mBroadHandle = 0;
  private:
    void _reassign(const Collider& collider, PhysicsObject* newOwner);

    SAlign ModelInstance mModelInst;
    int mFlags = 0;
    PhysicsObject* mOwner = nullptr;
  };
}