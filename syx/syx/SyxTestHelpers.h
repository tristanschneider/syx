#pragma once
#include "Precompile.h"

namespace Syx {
  extern bool TEST_FAILED;

  inline void FailTest(void) {
    TEST_FAILED = true;
  }

  template <typename T>
  void CheckResult(const T& a, const T& b) {
    if(a != b)
      FailTest();
  }

  inline void CheckResult(bool result) {
    if(!result)
      FailTest();
  }

  inline void CheckResult(float a, float b) {
    if(abs(a - b) > SYX_EPSILON)
      FailTest();
  }

  inline void CheckResult(const Vec3& lhs, const Vec3& rhs, float epsilon = SYX_EPSILON) {
    CheckResult(lhs.Equal(rhs, epsilon));
  }

  inline float FloatRand(float min, float max) {
    return min + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (max - min)));
  }

  inline Vec3 VecRand(float min, float max) {
    return Vec3(FloatRand(min, max), FloatRand(min, max), FloatRand(min, max));
  }

#ifdef SENABLED
  inline void CheckResult(SFloats a, SFloats b) {
    if(SVec3::Get(SVec3::Equal(a, b), 0) == 0.0f)
      FailTest();
  }

  inline void CheckResult(SFloats lhs, float x, float y, float z, float w, float epsilon) {
    SAlign float store[4];
    SStoreAll(store, lhs);
    if(!Vec3(store[0], store[1], store[2]).Equal(Vec3(x, y, z), epsilon) || abs(store[3]-w) > epsilon)
      FailTest();
  }

  inline void CheckResult(SFloats lhs, float x, float y, float z, float epsilon) {
    SAlign Vec3 result;
    SVec3::Store(lhs, result);
    if(!result.Equal(Vec3(x, y, z), epsilon))
      FailTest();
  }

  inline void CheckResult(SFloats lhs, float splat, float epsilon = SYX_EPSILON) {
    CheckResult(lhs, splat, splat, splat, epsilon);
  }

  inline void CheckResult(SFloats lhs, const Vec3& rhs, float epsilon = SYX_EPSILON) {
    CheckResult(lhs, rhs.x, rhs.y, rhs.z, epsilon);
  }

  inline void CheckResult(SFloats lhs, const Quat& rhs, float epsilon = SYX_EPSILON) {
    CheckResult(lhs, rhs.mV.x, rhs.mV.y, rhs.mV.z, rhs.mV.w, epsilon);
  }

  inline void CheckResult(const SMat3& lhs, const Mat3& rhs, float epsilon = SYX_EPSILON) {
    CheckResult(lhs.mbx, rhs.mbx, epsilon);
    CheckResult(lhs.mby, rhs.mby, epsilon);
    CheckResult(lhs.mbz, rhs.mbz, epsilon);
  }

  inline SFloats SVecRand(float min, float max) {
    Vec3 v = VecRand(min, max);
    return SLoadFloats(v.x, v.y, v.z, 0.0f);
  }
#endif
}