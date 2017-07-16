#pragma once
#include <utility>
#include "SyxMathDefines.h"
#include <cmath>

#include "SyxSIMD.h"
#include "SyxVec3.h"
#include "SyxSVec3.h"
#include "SyxMat3.h"
#include "SyxSMat3.h"
#include "SyxQuat.h"
#include "SyxSQuat.h"
#include "SyxVec2.h"
#include "SyxMat2.h"

namespace Syx {
  //Returns zero in case of division by zero
  template <typename Number>
  Number SafeDivide(const Number numerator, const Number denominator, const Number epsilon) {
    if(abs(denominator) < epsilon)
      return static_cast<Number>(0);
    return numerator/denominator;
  }

  template <typename T>
  T Clamp(T value, T min, T max, bool& clamped) {
    clamped = true;
    if(value > max)
      return max;
    if(value < min)
      return min;
    clamped = false;
    return value;
  }
  template <typename T>
  T Clamp(T value, T min, T max) {
    return std::min(std::max(value, min), max);
  }
  FInline float SignNum(float num) {
    return num > 0.0f ? 1.0f : -1.0f;
  }

  Vec3 TriangleNormal(const Vec3& a, const Vec3& b, const Vec3& c);
  float HalfPlaneD(const Vec3& normal, const Vec3& onPlane);
  float HalfPlaneSignedDistance(const Vec3& normal, float d, const Vec3& point);
  float HalfPlaneSignedDistance(const Vec3& normal, const Vec3& onPlane, const Vec3& point);
  Vec3 BarycentricToPoint(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& bary);
  //Result signed areas are (bcp, cap, abp)
  Vec3 PointToBarycentric(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& point);
  Vec3 PointToBarycentric(const Vec3& aToB, const Vec3& aToC, const Vec3& aToP);
  bool IsWithinTri(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& point, float epsilon = SYX_EPSILON);
  //Get the outward facing planes of the triangle formed by the given points. From there, dot4 can be used to get distances
  void GetOutwardTriPlanes(const Vec3& a, const Vec3& b, const Vec3& c, Vec3& resultA, Vec3& resultB, Vec3& resultC, bool normalized);
  float RandFloat(void);
  float RandFloat(float min, float max);
  Vec3 RandOnSphere(void);
  Mat3 TensorTransform(const Mat3& tensor, const Vec3& toPoint, float mass);
  Mat3 TensorTransform(const Mat3& tensor, const Mat3& rotation);
  //Given an ellipse at origin and line at origin, find intersection time of line with ellipse.
  float EllipseLineIntersect2d(const Vec2& line, const Vec2& ellipseScale);
  float EllipseLineIntersect2d(const Vec2& lineStart, const Vec2& lineDir, const Vec2& ellipseScale);
  //Given point on ellipse, adjust it to be the ellipse normal at that point
  void EllipsePointToNormal(const Vec2& point, const Vec2& ellipseScale, Vec2& normal);

#ifdef SENABLED
  SFloats STriangleNormal(SFloats a, SFloats b, SFloats c);
#endif

  template <typename Flag>
  void SetBits(Flag& flag, const Flag& bits, bool on) {
    if(on)
      flag |= bits;
    else
      flag &= ~bits;
  }

  template <typename T>
  bool Between(T in, T min, T max) {
    return min <= in && max >= in;
  }

  template <typename T>
  void OrderAscending(T& a, T& b, T& c) {
    if(a > b)
      std::swap(a, b);
    if(b > c)
      std::swap(b, c);
    if(a > b)
      std::swap(a, b);
    SyxAssertError(a <= b && b <= c);
  }

  inline void GetOtherIndices(int index, int& indexA, int& indexB) {
    //% is also an option, I don't know if that's faster
    switch(index) {
      case 0:
        indexA = 1;
        indexB = 2;
        break;
      case 1:
        indexA = 0;
        indexB = 2;
        break;
      case 2:
        indexA = 0;
        indexB = 1;
        break;
    }
  }

  template <typename Container>
  void SwapRemove(Container& container, int index) {
    std::swap(container[index], container[container.size() - 1]);
    container.pop_back();
  }

  //Validates that the given barycentric coordinates are all positive and add up to 1
  bool ValidBarycentric(const Vec3& bary);

  size_t CombineHash(size_t lhs, size_t rhs);

  template<typename LType, typename RType>
  struct PairHash {
    size_t operator()(const std::pair<LType, RType>& pair) const {
      std::hash<LType> lHash;
      std::hash<RType> rHash;
      return CombineHash(lHash(pair.first), rHash(pair.second));
    }
  };
}