#include "Precompile.h"
#include "CppUnitTest.h"

#include "IspcTestHolders.h"
#include "out_ispc/Inertia.h"
#include "out_ispc/Integrator.h"

#include "SyxMat3.h"
#include "SyxQuat.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace poa {
  TEST_CLASS(InertiaTests) {
    TEST_METHOD(Inertia_RecomputeInertiaTensor_IsUpdated) {
      Vec3Holder localInertiaHolder;
      QuatHolder orientationHolder;
      SymmetricMatrixHolder result;
      const auto orientation = Syx::Quat::axisAngle(Syx::Vec3(1, 0, 0), 2.f);
      const Syx::Vec3 localInertia(1, 2, 3);
      orientationHolder.set(0, orientation.mV.x, orientation.mV.y, orientation.mV.z, orientation.mV.w);
      localInertiaHolder.set(0, localInertia.x, localInertia.y, localInertia.z);
      const Syx::Mat3 expected = orientation.toMatrix().scaled(localInertia) * orientation.toMatrix().transposed();

      ispc::recomputeInertiaTensor(orientationHolder.mConstValue, localInertiaHolder.mConstValue, result.mValue, SIZE);

      const float e = 0.0001f;
      Assert::AreEqual(expected.get(0, 0), result.a[0], e);
      Assert::AreEqual(expected.get(0, 1), result.b[0], e);
      Assert::AreEqual(expected.get(0, 2), result.c[0], e);

      Assert::AreEqual(expected.get(1, 0), result.b[0], e);
      Assert::AreEqual(expected.get(1, 1), result.d[0], e);
      Assert::AreEqual(expected.get(1, 2), result.e[0], e);

      Assert::AreEqual(expected.get(2, 0), result.c[0], e);
      Assert::AreEqual(expected.get(2, 1), result.e[0], e);
      Assert::AreEqual(expected.get(2, 2), result.f[0], e);
    }

    TEST_METHOD(Inertia_ComputeSphereMass_IsComputed) {
      FloatArray inputRadius;
      FloatArray resultMass;
      Vec3Holder resultInertia;
      const float radius = 1.5f;
      const float pi = 3.14159265358979323846f;
      const float expectedMass = (4.0f*pi/3.0f)*radius*radius*radius;
      const float expectedInertia = (2.0f/5.0f)*radius*radius*expectedMass;
      inputRadius[0] = radius;

      ispc::computeSphereMass(inputRadius.data(), resultMass.data(), resultInertia.mValue, SIZE);

      Assert::AreEqual(expectedMass, resultMass[0], 0.0001f);
      Assert::AreEqual(expectedInertia, resultInertia.x[0], 0.0001f);
      Assert::AreEqual(expectedInertia, resultInertia.y[0], 0.0001f);
      Assert::AreEqual(expectedInertia, resultInertia.z[0], 0.0001f);
    }

    TEST_METHOD(Inertia_InvertMass_IsInverted) {
      FloatArray resultMass;
      FloatArray resultDensity;
      const float mass = 2.0f;
      const float density = 0.5f;
      resultMass[0] = resultMass[1] = mass;
      resultDensity[0] = density;
      resultDensity[1] = 0.0f;
      const float expectedInverseMass = 1.0f/(mass*density);

      ispc::invertMass(resultMass.data(), resultDensity.data(), SIZE);

      Assert::AreEqual(expectedInverseMass, resultMass[0], 0.0001f);
      Assert::AreEqual(0.0f, resultMass[1], 0.01f);
    }
  };
}