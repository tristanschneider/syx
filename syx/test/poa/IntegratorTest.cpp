#include "Precompile.h"
#include "CppUnitTest.h"

#include "out_ispc/Inertia.h"
#include "out_ispc/Integrator.h"

#include "SyxMat3.h"
#include "SyxQuat.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace poa {
  TEST_CLASS(IntegratorTests) {
    static constexpr inline uint32_t SIZE = 5;
    using FloatArray = std::array<float, size_t(SIZE)>;

    struct SymmetricMatrixHolder {
      FloatArray a, b, c, d, e, f;
      ispc::UniformSymmetricMatrix mValue{ a.data(), b.data(), c.data(), d.data(), e.data(), f.data() };
    };

    struct Vec3Holder {
      void set(size_t i, float a, float b, float c) {
        x[i] = a;
        y[i] = b;
        z[i] = c;
      }

      FloatArray x, y, z;
      ispc::UniformVec3 mValue{ x.data(), y.data(), z.data() };
    };

    struct QuatHolder {
      void set(size_t idx, float a, float b, float c, float d) {
        i[idx] = a;
        j[idx] = b;
        k[idx] = c;
        w[idx] = d;
      }

      FloatArray i, j, k, w;
      ispc::UniformQuat mValue{ i.data(), j.data(), k.data(), w.data() };
    };

    TEST_METHOD(Integrator_IntegratePosition_Integrates) {
      FloatArray pos, vel;
      pos[0] = 1;
      vel[0] = 2;
      const float dt = 0.5f;
      const float expected = pos[0] + vel[0]*dt;

      ispc::integrateLinearPosition(pos.data(), vel.data(), dt, SIZE);
      Assert::AreEqual(expected, pos[0], 0.00001f);
    }

    TEST_METHOD(Integrator_IntegrateRotation_Integrates) {
      QuatHolder resultOrientation;
      Vec3Holder resultAngVel;
      const auto orientation = Syx::Quat::axisAngle(Syx::Vec3(1, 0, 0), 2.f);
      const Syx::Vec3 angVel(1, 1, 1);
      const float dt = 0.5f;
      const Syx::Quat expected = (orientation + 0.5f*Syx::Quat(angVel.x, angVel.y, angVel.z, 0.0f)*orientation*dt).normalized();
      resultOrientation.i[0] = orientation.mV.x;
      resultOrientation.j[0] = orientation.mV.y;
      resultOrientation.k[0] = orientation.mV.z;
      resultOrientation.w[0] = orientation.mV.w;
      resultAngVel.x[0] = angVel.x;
      resultAngVel.y[0] = angVel.y;
      resultAngVel.z[0] = angVel.z;

      ispc::integrateRotation(resultOrientation.mValue,  resultAngVel.mValue, dt, SIZE);

      constexpr float e = 0.0001f;
      Assert::AreEqual(expected.mV.x, resultOrientation.i[0], e);
      Assert::AreEqual(expected.mV.y, resultOrientation.j[0], e);
      Assert::AreEqual(expected.mV.z, resultOrientation.k[0], e);
      Assert::AreEqual(expected.mV.w, resultOrientation.w[0], e);
    }

    TEST_METHOD(Integrator_IntegrateAccelleration_Integrates) {
      FloatArray velocity;
      const float gravity = -9.8f;
      const float dt = 0.5f;
      velocity[0] = 1.f;
      const float expected = velocity[0] + gravity*dt;

      ispc::integrateLinearVelocityGlobalAccelleration(velocity.data(), gravity, dt, SIZE);

      Assert::AreEqual(expected, velocity[0], 0.0001f);
    }

    TEST_METHOD(Integrator_UpdateInertia_IsUpdated) {
      Vec3Holder localInertiaHolder;
      QuatHolder orientationHolder;
      SymmetricMatrixHolder result;
      const auto orientation = Syx::Quat::axisAngle(Syx::Vec3(1, 0, 0), 2.f);
      const Syx::Vec3 localInertia(1, 2, 3);
      orientationHolder.set(0, orientation.mV.x, orientation.mV.y, orientation.mV.z, orientation.mV.w);
      localInertiaHolder.set(0, localInertia.x, localInertia.y, localInertia.z);
      const Syx::Mat3 expected = orientation.toMatrix().scaled(localInertia) * orientation.toMatrix().transposed();

      ispc::recomputeInertiaTensor(orientationHolder.mValue, localInertiaHolder.mValue, result.mValue, SIZE);

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
  };
}