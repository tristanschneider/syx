#include "Precompile.h"
#include "CppUnitTest.h"

#include "IspcTestHolders.h"
#include "out_ispc/CollisionDetection.h"
#include "out_ispc/Inertia.h"
#include "out_ispc/Integrator.h"

#include "SyxMat3.h"
#include "SyxQuat.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace poa {
  TEST_CLASS(CollisionDetectionTests) {
    TEST_METHOD(SphereSphereContact_ComputeContact_IsContact) {
      Vec3Holder posA, posB, resultContact, resultNormal;
      FloatArray resultOverlap, radiusA, radiusB;
      const float base = 1.0f;
      const float radA = 0.5f;
      const float radB = 0.75f;
      const float expectedOverlap = 0.1f;
      const float distance = radA + radB - expectedOverlap;
      posA.set(0, base, 0, 0);
      posB.set(0, base + distance, 0, 0);
      radiusA[0] = radA;
      radiusB[0] = radB;

      ispc::computeContactSphereSphere(posA.mValue, posB.mValue, radiusA.data(), radiusB.data(), resultContact.mValue, resultNormal.mValue, resultOverlap.data(), SIZE);

      Assert::AreEqual(base + radA, resultContact.x[0], 0.001f);
      Assert::AreEqual(0, resultContact.y[0], 0.001f);
      Assert::AreEqual(0, resultContact.z[0], 0.001f);

      Assert::AreEqual(-1.0f, resultNormal.x[0], 0.001f);
      Assert::AreEqual(0.0f, resultNormal.y[0], 0.001f);
      Assert::AreEqual(0.0f, resultNormal.z[0], 0.001f);

      Assert::AreEqual(expectedOverlap, resultOverlap[0], 0.001f);
    }

    TEST_METHOD(SphereSphereNoContact_ComputeContact_NegativeOverlap) {
      Vec3Holder posA, posB, resultContact, resultNormal;
      FloatArray resultOverlap, radiusA, radiusB;
      const float base = 1.0f;
      const float radA = 0.5f;
      const float radB = 0.75f;
      const float expectedOverlap = -0.1f;
      const float distance = radA + radB - expectedOverlap;
      posA.set(0, base, 0, 0);
      posB.set(0, base + distance, 0, 0);
      radiusA[0] = radA;
      radiusB[0] = radB;

      ispc::computeContactSphereSphere(posA.mValue, posB.mValue, radiusA.data(), radiusB.data(), resultContact.mValue, resultNormal.mValue, resultOverlap.data(), SIZE);

      Assert::AreEqual(expectedOverlap, resultOverlap[0], 0.001f);
    }

    TEST_METHOD(SphereSphereOverlapping_ComputeContact_UsesXAxis) {
      Vec3Holder posA, posB, resultContact, resultNormal;
      FloatArray resultOverlap, radiusA, radiusB;
      const float base = 1.0f;
      const float radA = 0.5f;
      const float radB = 0.75f;
      const float expectedOverlap = radA + radB;
      const float distance = radA + radB - expectedOverlap;
      posA.set(0, base, 0, 0);
      posB.set(0, base + distance, 0, 0);
      radiusA[0] = radA;
      radiusB[0] = radB;

      ispc::computeContactSphereSphere(posA.mValue, posB.mValue, radiusA.data(), radiusB.data(), resultContact.mValue, resultNormal.mValue, resultOverlap.data(), SIZE);

      Assert::AreEqual(base - radA, resultContact.x[0], 0.001f);
      Assert::AreEqual(0, resultContact.y[0], 0.001f);
      Assert::AreEqual(0, resultContact.z[0], 0.001f);

      Assert::AreEqual(1.0f, resultNormal.x[0], 0.001f);
      Assert::AreEqual(0.0f, resultNormal.y[0], 0.001f);
      Assert::AreEqual(0.0f, resultNormal.z[0], 0.001f);

      Assert::AreEqual(expectedOverlap, resultOverlap[0], 0.001f);
    }
  };
}