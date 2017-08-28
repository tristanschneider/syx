#pragma once
namespace Syx {
  //Returns zero in case of division by zero
  template <typename Number>
  Number safeDivide(const Number numerator, const Number denominator, const Number epsilon) {
    if(std::abs(denominator) < epsilon)
      return static_cast<Number>(0);
    return numerator/denominator;
  }

  template <typename T>
  T clamp(T value, T min, T max, bool& clamped) {
    clamped = true;
    if(value > max)
      return max;
    if(value < min)
      return min;
    clamped = false;
    return value;
  }
  template <typename T>
  T clamp(T value, T min, T max) {
    return std::min(std::max(value, min), max);
  }
  FInline float signNum(float num) {
    return num > 0.0f ? 1.0f : -1.0f;
  }

  Vec3 triangleNormal(const Vec3& a, const Vec3& b, const Vec3& c);
  float halfPlaneD(const Vec3& normal, const Vec3& onPlane);
  float halfPlaneSignedDistance(const Vec3& normal, float d, const Vec3& point);
  float halfPlaneSignedDistance(const Vec3& normal, const Vec3& onPlane, const Vec3& point);
  Vec3 barycentricToPoint(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& bary);
  //Result signed areas are (bcp, cap, abp)
  Vec3 pointToBarycentric(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& point);
  Vec3 pointToBarycentric(const Vec3& aToB, const Vec3& aToC, const Vec3& aToP);
  bool isWithinTri(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& point, float epsilon = SYX_EPSILON);
  //Get the outward facing planes of the triangle formed by the given points. From there, dot4 can be used to get distances
  void getOutwardTriPlanes(const Vec3& a, const Vec3& b, const Vec3& c, Vec3& resultA, Vec3& resultB, Vec3& resultC, bool normalized);
  float randFloat(void);
  float randFloat(float min, float max);
  Vec3 randOnSphere(void);
  Mat3 tensorTransform(const Mat3& tensor, const Vec3& toPoint, float mass);
  Mat3 tensorTransform(const Mat3& tensor, const Mat3& rotation);
  //Given an ellipse at origin and line at origin, find intersection time of line with ellipse.
  float ellipseLineIntersect2d(const Vec2& line, const Vec2& ellipseScale);
  float ellipseLineIntersect2d(const Vec2& lineStart, const Vec2& lineDir, const Vec2& ellipseScale);
  //Given point on ellipse, adjust it to be the ellipse normal at that point
  void ellipsePointToNormal(const Vec2& point, const Vec2& ellipseScale, Vec2& normal);

#ifdef SENABLED
  SFloats sTriangleNormal(SFloats a, SFloats b, SFloats c);
#endif

  template <typename Flag>
  void setBits(Flag& flag, const Flag& bits, bool on) {
    if(on)
      flag |= bits;
    else
      flag &= ~bits;
  }

  template <typename T>
  bool between(T in, T min, T max) {
    return min <= in && max >= in;
  }

  template <typename T>
  void orderAscending(T& a, T& b, T& c) {
    if(a > b)
      std::swap(a, b);
    if(b > c)
      std::swap(b, c);
    if(a > b)
      std::swap(a, b);
    SyxAssertError(a <= b && b <= c);
  }

  inline void getOtherIndices(int index, int& indexA, int& indexB) {
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
  void swapRemove(Container& container, int index) {
    std::swap(container[index], container[container.size() - 1]);
    container.pop_back();
  }

  //Validates that the given barycentric coordinates are all positive and add up to 1
  bool validBarycentric(const Vec3& bary);

  size_t combineHash(size_t lhs, size_t rhs);

  template<typename LType, typename RType>
  struct PairHash {
    size_t operator()(const std::pair<LType, RType>& pair) const {
      std::hash<LType> lHash;
      std::hash<RType> rHash;
      return combineHash(lHash(pair.first), rHash(pair.second));
    }
  };
}