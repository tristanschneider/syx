#include "Precompile.h"
#include "SyxCollider.h"
#include "SyxPhysicsSystem.h"
#include "SyxSpace.h"

namespace Syx {
  Collider::Collider(PhysicsObject* owner): mOwner(owner), mFlags(0), mBroadHandle(SyxInvalidHandle) {
  }

  void Collider::SetFlag(int flag, bool value) {
    SetBits(mFlags, flag, value);
  }

  bool Collider::GetFlag(int flag) {
    return (mFlags & flag) != 0;
  }

  Vec3 Collider::GetSupport(const Vec3& dir) {
    return mModelInst.GetSupport(dir);
  }

  void Collider::UpdateModelInst(const Transform& parentTransform) {
    mModelInst.UpdateTransformers(parentTransform);
    mModelInst.UpdateAABB();
  }

  PhysicsObject* Collider::GetOwner(void) {
    return mOwner;
  }

  void Collider::SetModel(const Model& model) {
    mModelInst.SetModel(model);
  }

  void Collider::SetMaterial(const Material& material) {
    mModelInst.SetMaterial(material);
  }

  int Collider::GetModelType(void) {
    return mModelInst.GetModelType();
  }

  void Collider::Initialize(Space& space) {
    mBroadHandle = space.mBroadphase->Insert(BoundingVolume(GetAABB()), reinterpret_cast<void*>(mOwner));
  }

  void Collider::Uninitialize(Space& space) {
    space.mBroadphase->Remove(mBroadHandle);
    mBroadHandle = SyxInvalidHandle;
  }

#ifdef SENABLED
  SFloats Collider::SGetSupport(SFloats dir) {
    return mModelInst.SGetSupport(dir);
  }
#else

#endif
}