#pragma once
#include <utility>
#include "SyxMathDefines.h"
#include <cmath>

#include "SyxSIMD.h"
#include "SyxVector3.h"
#include "SyxSVector3.h"
#include "SyxMatrix3.h"
#include "SyxSMatrix3.h"
#include "SyxQuaternion.h"
#include "SyxSQuaternion.h"
#include "SyxVector2.h"
#include "SyxMatrix2.h"

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

  Vector3 TriangleNormal(const Vector3& a, const Vector3& b, const Vector3& c);
  float HalfPlaneD(const Vector3& normal, const Vector3& onPlane);
  float HalfPlaneSignedDistance(const Vector3& normal, float d, const Vector3& point);
  float HalfPlaneSignedDistance(const Vector3& normal, const Vector3& onPlane, const Vector3& point);
  Vector3 BarycentricToPoint(const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& bary);
  //Result signed areas are (bcp, cap, abp)
  Vector3 PointToBarycentric(const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& point);
  Vector3 PointToBarycentric(const Vector3& aToB, const Vector3& aToC, const Vector3& aToP);
  bool IsWithinTri(const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& point, float epsilon = SYX_EPSILON);
  //Get the outward facing planes of the triangle formed by the given points. From there, dot4 can be used to get distances
  void GetOutwardTriPlanes(const Vector3& a, const Vector3& b, const Vector3& c, Vector3& resultA, Vector3& resultB, Vector3& resultC, bool normalized);
  float RandFloat(void);
  float RandFloat(float min, float max);
  Vector3 RandOnSphere(void);
  Matrix3 TensorTransform(const Matrix3& tensor, const Vector3& toPoint, float mass);
  Matrix3 TensorTransform(const Matrix3& tensor, const Matrix3& rotation);
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
  bool ValidBarycentric(const Vector3& bary);

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