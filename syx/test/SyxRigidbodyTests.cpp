#include "Precompile.h"
#include "CppUnitTest.h"

#include "SyxPhysicsObject.h"
#include "SyxCollider.h"
#include "SyxModel.h"
#include "SyxRigidbody.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Syx;

namespace SyxTests {
  TEST_CLASS(RigidbodyTests) {
    struct MockPhysicsObject {
      MockPhysicsObject()
        : mObj(0, mHandle)
        , mModel(std::make_shared<Model>(ModelType::Cube))
      {
        mObj.setRigidbodyEnabled(true);
        mObj.setColliderEnabled(true);
        mObj.getCollider()->setModel(mModel);
        mObj.getRigidbody()->calculateMass();
      }

      Rigidbody* operator->() {
        return mObj.getRigidbody();
      }

      PhysicsObject::HandleT mHandle;
      PhysicsObject mObj;
      std::shared_ptr<Model> mModel;
    };

    TEST_METHOD(Rigidbody_ApplyAngularImpulse_AngularVelocityModified) {
      MockPhysicsObject obj;

      obj->applyImpulse(Vec3::Zero, Vec3::UnitY, nullptr);

      Assert::IsTrue(obj->mAngVel.y > 0.f, L"Angular velocity should have been modified by impulse", LINE_INFO());
    }

    TEST_METHOD(YRotationLockedRigidbody_ApplyAngularImpulse_Unaffected) {
      MockPhysicsObject obj;
      obj->setFlag(RigidbodyFlags::LockAngY, true);

      obj->applyImpulse(Vec3::Zero, Vec3::UnitY, nullptr);

      Assert::AreEqual(0.f, obj->mAngVel.y, L"Locked axis should not rotate", LINE_INFO());
    }

    TEST_METHOD(AllRotationLockedRigidbody_ApplyAngularImpulse_Unaffected) {
      MockPhysicsObject obj;
      obj->setFlag(RigidbodyFlags::LockAngY | RigidbodyFlags::LockAngX | RigidbodyFlags::LockAngZ, true);

      obj->applyImpulse(Vec3::Zero, Vec3(1.f), nullptr);

      Assert::AreEqual(0.f, obj->mAngVel.x, L"Locked axis should not rotate", LINE_INFO());
      Assert::AreEqual(0.f, obj->mAngVel.y, L"Locked axis should not rotate", LINE_INFO());
      Assert::AreEqual(0.f, obj->mAngVel.z, L"Locked axis should not rotate", LINE_INFO());
    }
  };
}