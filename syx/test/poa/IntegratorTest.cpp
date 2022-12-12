#include "Precompile.h"
#include "CppUnitTest.h"

#include "IspcTestHolders.h"
#include "out_ispc/unity.h"

#include "SyxMat3.h"
#include "SyxQuat.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace poa {
  TEST_CLASS(IntegratorTests) {

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

      ispc::integrateRotation(resultOrientation.mValue,  resultAngVel.mConstValue, dt, SIZE);

      constexpr float e = 0.0001f;
      Assert::AreEqual(expected.mV.x, resultOrientation.i[0], e);
      Assert::AreEqual(expected.mV.y, resultOrientation.j[0], e);
      Assert::AreEqual(expected.mV.z, resultOrientation.k[0], e);
      Assert::AreEqual(expected.mV.w, resultOrientation.w[0], e);
    }

    TEST_METHOD(Integrator_IntegrateAcceleration_Integrates) {
      FloatArray velocity;
      const float gravity = -9.8f;
      const float dt = 0.5f;
      velocity[0] = 1.f;
      const float expected = velocity[0] + gravity*dt;

      ispc::integrateLinearVelocityGlobalAcceleration(velocity.data(), gravity, dt, SIZE);

      Assert::AreEqual(expected, velocity[0], 0.0001f);
    }
  };
}