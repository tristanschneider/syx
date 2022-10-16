#include "Precompile.h"
#include "CppUnitTest.h"

#include "out_ispc/Integrator.h"

#include "SyxMat3.h"
#include "SyxQuat.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace poa {
  TEST_CLASS(IntegratorTests) {
    static constexpr inline uint32_t SIZE = 5;
    using FloatArray = std::array<float, size_t(SIZE)>;
    using MatrixRow = std::array<ispc::MatrixRow, size_t(SIZE)>;

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
      FloatArray orientationI, orientationJ, orientationK, orientationW,
        angVelX, angVelY, angVelZ;
      const auto orientation = Syx::Quat::axisAngle(Syx::Vec3(1, 0, 0), 2.f);
      const Syx::Vec3 angVel(1, 1, 1);
      const float dt = 0.5f;
      const Syx::Quat expected = (orientation + 0.5f*Syx::Quat(angVel.x, angVel.y, angVel.z, 0.0f)*orientation*dt).normalized();
      orientationI[0] = orientation.mV.x;
      orientationJ[0] = orientation.mV.y;
      orientationK[0] = orientation.mV.z;
      orientationW[0] = orientation.mV.w;
      angVelX[0] = angVel.x;
      angVelY[0] = angVel.y;
      angVelZ[0] = angVel.z;

      ispc::integrateRotation(orientationI.data(),
        orientationJ.data(),
        orientationK.data(),
        orientationW.data(),
        angVelX.data(),
        angVelY.data(),
        angVelZ.data(),
        dt,
        SIZE);

      constexpr float e = 0.0001f;
      Assert::AreEqual(expected.mV.x, orientationI[0], e);
      Assert::AreEqual(expected.mV.y, orientationJ[0], e);
      Assert::AreEqual(expected.mV.z, orientationK[0], e);
      Assert::AreEqual(expected.mV.w, orientationW[0], e);
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
      FloatArray orientationI, orientationJ, orientationK, orientationW,
        localInertiaX, localInertiaY, localInertiaZ;
      MatrixRow resultA, resultB, resultC;
      const auto orientation = Syx::Quat::axisAngle(Syx::Vec3(1, 0, 0), 2.f);
      const Syx::Vec3 localInertia(1, 2, 3);
      orientationI[0] = orientation.mV.x;
      orientationJ[0] = orientation.mV.y;
      orientationK[0] = orientation.mV.z;
      orientationW[0] = orientation.mV.w;
      localInertiaX[0] = localInertia.x;
      localInertiaY[0] = localInertia.y;
      localInertiaZ[0] = localInertia.z;
      const Syx::Mat3 expected = orientation.toMatrix().scaled(localInertia) * orientation.toMatrix().transposed();

      ispc::recomputeInertiaTensor(orientationI.data(),
        orientationJ.data(),
        orientationK.data(),
        orientationW.data(),
        localInertiaX.data(),
        localInertiaY.data(),
        localInertiaZ.data(),
        (ispc::MatrixRow*)resultA.data(),
        (ispc::MatrixRow*)resultB.data(),
        (ispc::MatrixRow*)resultC.data(),
        SIZE
      );

      const float e = 0.0001f;
      Assert::AreEqual(expected.get(0, 0), resultA[0].x, e);
      Assert::AreEqual(expected.get(0, 1), resultA[0].y, e);
      Assert::AreEqual(expected.get(0, 2), resultA[0].z, e);

      Assert::AreEqual(expected.get(1, 0), resultB[0].x, e);
      Assert::AreEqual(expected.get(1, 1), resultB[0].y, e);
      Assert::AreEqual(expected.get(1, 2), resultB[0].z, e);

      Assert::AreEqual(expected.get(2, 0), resultC[0].x, e);
      Assert::AreEqual(expected.get(2, 1), resultC[0].y, e);
      Assert::AreEqual(expected.get(2, 2), resultC[0].z, e);
    }
  };
}