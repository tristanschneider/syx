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
    //Don't want to copy this way because of owner*
    Collider(const Collider&) = delete;
    Collider& operator=(const Collider&) = delete;

    void Initialize(Space& space);
    void Uninitialize(Space& space);

    PhysicsObject* GetOwner(void);

    void SetFlag(int flag, bool value);
    bool GetFlag(int flag);
    Vector3 GetSupport(const Vector3& dir);
    SFloats SGetSupport(SFloats dir);

    void UpdateModelInst(const Transform& parentTransform);

    void SetModel(const Model& model);
    void SetMaterial(const Material& material);

    int GetModelType(void);

    const Model& GetModel(void) { return mModelInst.GetModel(); }
    ModelInstance& GetModelInstance(void) { return mModelInst; }
    const AABB& GetAABB(void) { return mModelInst.GetAABB(); }

    Handle mBroadHandle;
  private:
    SAlign ModelInstance mModelInst;
    int mFlags;
    PhysicsObject* mOwner;
  };
}