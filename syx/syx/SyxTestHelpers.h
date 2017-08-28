#pragma once
#include "Precompile.h"

namespace Syx {
  extern bool TEST_FAILED;

  inline void failTest(void) {
    TEST_FAILED = true;
  }

  template <typename T>
  void checkResult(const T& a, const T& b) {
    if(a != b)
      failTest();
  }

  inline void checkResult(bool result) {
    if(!result)
      failTest();
  }

  inline void checkResult(float a, float b) {
    if(abs(a - b) > SYX_EPSILON)
      failTest();
  }

  inline void checkResult(const Vec3& lhs, const Vec3& rhs, float epsilon = SYX_EPSILON) {
    checkResult(lhs.equal(rhs, epsilon));
  }

  inline float floatRand(float min, float max) {
    return min + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (max - min)));
  }

  inline Vec3 vecRand(float min, float max) {
    return Vec3(floatRand(min, max), floatRand(min, max), floatRand(min, max));
  }

#ifdef SENABLED
  inline void checkResult(SFloats a, SFloats b) {
    if(SVec3::get(SVec3::equal(a, b), 0) == 0.0f)
      failTest();
  }

  inline void checkResult(SFloats lhs, float x, float y, float z, float w, float epsilon) {
    SAlign float store[4];
    SStoreAll(store, lhs);
    if(!Vec3(store[0], store[1], store[2]).equal(Vec3(x, y, z), epsilon) || abs(store[3]-w) > epsilon)
      failTest();
  }

  inline void checkResult(SFloats lhs, float x, float y, float z, float epsilon) {
    SAlign Vec3 result;
    SVec3::store(lhs, result);
    if(!result.equal(Vec3(x, y, z), epsilon))
      failTest();
  }

  inline void checkResult(SFloats lhs, float splat, float epsilon = SYX_EPSILON) {
    checkResult(lhs, splat, splat, splat, epsilon);
  }

  inline void checkResult(SFloats lhs, const Vec3& rhs, float epsilon = SYX_EPSILON) {
    checkResult(lhs, rhs.x, rhs.y, rhs.z, epsilon);
  }

  inline void checkResult(SFloats lhs, const Quat& rhs, float epsilon = SYX_EPSILON) {
    checkResult(lhs, rhs.mV.x, rhs.mV.y, rhs.mV.z, rhs.mV.w, epsilon);
  }

  inline void checkResult(const SMat3& lhs, const Mat3& rhs, float epsilon = SYX_EPSILON) {
    checkResult(lhs.mbx, rhs.mbx, epsilon);
    checkResult(lhs.mby, rhs.mby, epsilon);
    checkResult(lhs.mbz, rhs.mbz, epsilon);
  }

  inline SFloats sVecRand(float min, float max) {
    Vec3 v = vecRand(min, max);
    return sLoadFloats(v.x, v.y, v.z, 0.0f);
  }
#endif
}