#include "Precompile.h"
#include "SyxCollider.h"
#include "SyxPhysicsSystem.h"
#include "SyxSpace.h"

namespace Syx {
  Collider::Collider(PhysicsObject* owner): mOwner(owner), mFlags(0), mBroadHandle(SyxInvalidHandle) {
  }

  void Collider::setFlag(int flag, bool value) {
    setBits(mFlags, flag, value);
  }

  bool Collider::getFlag(int flag) {
    return (mFlags & flag) != 0;
  }

  Vec3 Collider::getSupport(const Vec3& dir) {
    return mModelInst.getSupport(dir);
  }

  void Collider::updateModelInst(const Transform& parentTransform) {
    mModelInst.updateTransformers(parentTransform);
    mModelInst.updateAABB();
  }

  PhysicsObject* Collider::getOwner(void) {
    return mOwner;
  }

  void Collider::setModel(std::shared_ptr<const Model> model) {
    mModelInst.setModel(std::move(model));
  }

  void Collider::setModel(std::shared_ptr<const Model> model, Space& space) {
    setModel(std::move(model));
    if(PhysicsObject* owner = getOwner()) {
      space.updateMovedObject(*owner);
      if(Rigidbody* rigidbody = owner->getRigidbody()) {
        rigidbody->calculateMass();
      }
    }
  }

  void Collider::setMaterial(const IMaterialHandle& material) {
    mModelInst.setMaterial(material);
    if(Rigidbody* rb = mOwner ? mOwner->getRigidbody() : nullptr) {
      rb->calculateMass();
    }
  }

  int Collider::getModelType(void) {
    return mModelInst.getModelType();
  }

  void Collider::initialize(Space& space) {
    mBroadHandle = space.mBroadphase->insert(BoundingVolume(getAABB()), reinterpret_cast<void*>(mOwner));
  }

  void Collider::uninitialize(Space& space) {
    space.mBroadphase->remove(mBroadHandle);
    mBroadHandle = SyxInvalidHandle;
  }

#ifdef SENABLED
  SFloats Collider::sGetSupport(SFloats dir) {
    return mModelInst.sGetSupport(dir);
  }
#else

#endif
}