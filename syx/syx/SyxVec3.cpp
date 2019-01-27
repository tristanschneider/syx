#include "Precompile.h"

namespace Syx {
  SAlign const Vec3 Vec3::UnitX(1.0f, 0.0f, 0.0f);
  SAlign const Vec3 Vec3::UnitY(0.0f, 1.0f, 0.0f);
  SAlign const Vec3 Vec3::UnitZ(0.0f, 0.0f, 1.0f);
  SAlign const Vec3 Vec3::Zero(0.0f);
  SAlign const Vec3 Vec3::Identity(1.0f);

  Vec3 Vec3::operator+(const Vec3& rhs) const {
    return Vec3(x + rhs.x, y + rhs.y, z + rhs.z);
  }

  Vec3 Vec3::operator-(const Vec3& rhs) const {
    return Vec3(x - rhs.x, y - rhs.y, z - rhs.z);
  }

  Vec3 Vec3::operator-(void) const {
    return Vec3(-x, -y, -z);
  }

  Vec3 Vec3::operator*(float rhs) const {
    return Vec3(x*rhs, y*rhs, z*rhs);
  }

  Vec3 Vec3::operator/(float rhs) const {
    float divisor = 1.0f/rhs;
    return divisor**this;
  }

  const float& Vec3::operator[](int index) const {
    return *(&x + index);
  }

  float& Vec3::operator[](int index) {
    return *(&x + index);
  }

  Vec3& Vec3::operator+=(const Vec3& rhs) {
    x += rhs.x;
    y += rhs.y;
    z += rhs.z;
    return *this;
  }

  Vec3& Vec3::operator-=(const Vec3& rhs) {
    x -= rhs.x;
    y -= rhs.y;
    z -= rhs.z;
    return *this;
  }

  Vec3& Vec3::operator*=(float rhs) {
    x *= rhs;
    y *= rhs;
    z *= rhs;
    return *this;
  }

  Vec3& Vec3::operator/=(float rhs) {
    float divisor = 1.0f/rhs;
    return *this *= divisor;
  }

  bool Vec3::operator==(const Vec3& rhs) const {
    return equal(rhs, SYX_EPSILON);
  }

  bool Vec3::operator!=(const Vec3& rhs) const {
    return !equal(rhs, SYX_EPSILON);
  }

  bool Vec3::equal(const Vec3& rhs, float epsilon) const {
    return std::abs(x - rhs.x) <= epsilon &&
      std::abs(y - rhs.y) <= epsilon &&
      std::abs(z - rhs.z) <= epsilon;
  }

  //From Erin Catto's box2d forum. Assumes normalized this
  Vec3 Vec3::getOrthogonal() const {
    //Suppose vector a has all equal components and is a unit vector: a = (s, s, s)
    //Then 3*s*s = 1, s = sqrt(1/3) = 0.57735. This means that at least one component of a
    //unit vector must be greater or equal to 0.57735.
    return std::abs(x) >= 0.57735f ? Vec3(y, -x, 0.0f).normalized() : Vec3(0.0f, z, -y).normalized();
  }

  //Assumes normalized this
  void Vec3::getBasis(Vec3& resultA, Vec3& resultB) const {
    resultA = getOrthogonal();
    resultB = cross(resultA);
  }

  float Vec3::length(void) const {
    return sqrt(length2());
  }

  float Vec3::length2(void) const {
    return dot(*this);
  }

  float Vec3::distance(const Vec3& other) const {
    return sqrt(distance2(other));
  }

  float Vec3::distance2(const Vec3& other) const {
    return (*this - other).length2();
  }

  float Vec3::dot(const Vec3& other) const {
    return x*other.x + y*other.y + z*other.z;
  }

  float Vec3::dot4(const Vec3& other) const {
    return dot(other) + w;
  }

  Vec3 Vec3::cross(const Vec3& other) const {
    return Vec3(y*other.z - z*other.y,
      z*other.x - x*other.z,
      x*other.y - y*other.x);
  }

  //Returns index of least significant, so x = 0, y = 1, etc.
  int Vec3::leastSignificantAxis(void) const {
    Vec3 absIn = abs(*this);
    if(absIn.x < absIn.y) {
      if(absIn.x < absIn.z)
        return 0; // X < Y < Z, X < Z < Y
      return 2; // Z < X < Y
    }
    else if(absIn.y < absIn.z)
      return 1; // Y < X < Z, Y < Z < X
    return 2; // Z < Y < X
  }

  int Vec3::mostSignificantAxis(void) const {
    Vec3 absIn = abs(*this);
    if(absIn.x > absIn.y) {
      if(absIn.x > absIn.z)
        return 0; // X > Y > Z, X > Z > Y
      return 2; // Z > X > Y
    }
    else if(absIn.y > absIn.z)
      return 1; // Y > X > Z, Y > Z > X
    return 2; // Z > Y > X
  }

  Vec3 Vec3::normalized(void) const {
    return *this/length();
  }

  Vec3 Vec3::safeNormalized(void) const {
    return safeDivide(*this, length());
  }

  const float* Vec3::data() const {
    return &x;
  }

  float* Vec3::data() {
    return &x;
  }

  void Vec3::safeDivide(float rhs) {
    float scalar = 0.0f;
    if(std::abs(rhs) > SYX_EPSILON)
      scalar = 1.0f/rhs;
    *this *= scalar;
  }

  void Vec3::scale(const Vec3& rhs) {
    x *= rhs.x;
    y *= rhs.y;
    z *= rhs.z;
  }

  void Vec3::lerp(const Vec3& end, float t) {
    *this = *this + t*(end - *this);
  }

  void Vec3::abs(void) {
    x = std::abs(x);
    y = std::abs(y);
    z = std::abs(z);
  }

  void Vec3::projVec(const Vec3& onto) {
    *this = onto*(dot(onto)/onto.length2());
  }

  void Vec3::pointPlaneProj(const Vec3& normal, const Vec3& onPlane) {
    Vec3 proj = projVec(*this - onPlane, normal);
    *this -= proj;
  }

  void Vec3::normalize(void) {
    *this /= length();
  }

  void Vec3::safeNormalize(void) {
    safeDivide(length());
  }

  Vec3 Vec3::reciprocal(void) const {
    return Vec3(Syx::safeDivide(1.0f, x, SYX_EPSILON), Syx::safeDivide(1.0f, y, SYX_EPSILON), Syx::safeDivide(1.0f, z, SYX_EPSILON));
  }

  Vec3 Vec3::reciprocal4(void) const {
    return Vec3(Syx::safeDivide(1.0f, x, SYX_EPSILON),
      Syx::safeDivide(1.0f, y, SYX_EPSILON),
      Syx::safeDivide(1.0f, z, SYX_EPSILON),
      Syx::safeDivide(1.0f, w, SYX_EPSILON));
  }

  Vec3 Vec3::mat2Inversed() const {
    //  1   [w ,-y]
    //xw-yz [-z, x]
    float invdet = Syx::safeDivide(1.0f, x*w - y*z, SYX_EPSILON);
    return Vec3(w*invdet, -y*invdet,
                -z*invdet, x*invdet);
  }

  Vec3 Vec3::mat2Mul(const Vec3& v) const {
    //[x y]*[x]
    //[z w] [y]
    return Vec3(x*v.x + y*v.y, z*v.x + w*v.y, 0.0f);
  }

  Vec3 Vec3::mat2MatMul(const Vec3& mat) const {
    //[x y]*[x y]
    //[z w] [z w]
    return Vec3(x*mat.x + y*mat.z, x*mat.y + y*mat.w,
                z*mat.x + w*mat.z, z*mat.y + w*mat.w);
  }

  Vec3 Vec3::safeDivide(const Vec3& vec, float div) {
    float scalar = 0.0f;
    if(std::abs(div) > SYX_EPSILON)
      scalar = 1.0f/div;
    return scalar*vec;
  }

  Vec3 Vec3::scale(const Vec3& lhs, const Vec3& rhs) {
    return Vec3(lhs.x*rhs.x, lhs.y*rhs.y, lhs.z*rhs.z);
  }

  Vec3 Vec3::lerp(const Vec3& start, const Vec3& end, float t) {
    return start + t*(end - start);
  }

  Vec3 Vec3::abs(const Vec3& in) {
    return Vec3(std::abs(in.x), std::abs(in.y), std::abs(in.z));
  }

  float Vec3::pointLineDistanceSQ(const Vec3& point, const Vec3& start, const Vec3& end) {
    Vec3 projPoint = start + projVec(point - start, end - start);
    return length2(point - projPoint);
  }

  Vec3 Vec3::ccwTriangleNormal(const Vec3& a, const Vec3& b, const Vec3& c) {
    return (b - a).cross(c - a);
  }

  Vec3 Vec3::perpendicularLineToPoint(const Vec3& line, const Vec3& lineToPoint) {
    return cross(cross(line, lineToPoint), line);
  }

  Vec3 Vec3::barycentricToPoint(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& barycentric) {
    return a*barycentric.x + b*barycentric.y + c*barycentric.z;
  }

  Vec3 Vec3::projVec(const Vec3& vec, const Vec3& onto) {
    return onto*(dot(vec, onto)/length2(onto));
  }

  float Vec3::projVecScalar(const Vec3& vec, const Vec3& onto) {
    return dot(vec, onto)/dot(onto, onto);
  }

  Vec3 Vec3::pointPlaneProj(const Vec3& point, const Vec3& normal, const Vec3& onPlane) {
    return point - projVec(point - onPlane, normal);
  }

  float Vec3::getScalarT(const Vec3& start, const Vec3& end, const Vec3& pointOnLine) {
    Vec3 startToEnd = end - start;
    unsigned nonZeroAxis = 0;
    for(unsigned i = 0; i < 3; ++i)
      if(std::abs(startToEnd[i]) > SYX_EPSILON) {
        nonZeroAxis = i;
        break;
      }

    return (pointOnLine[nonZeroAxis] - start[nonZeroAxis]) / startToEnd[nonZeroAxis];
  }

  float Vec3::length(const Vec3& in) {
    return in.length();
  }

  float Vec3::length2(const Vec3& in) {
    return in.length2();
  }

  float Vec3::distance(const Vec3& lhs, const Vec3& rhs) {
    return lhs.distance(rhs);
  }

  float Vec3::distance2(const Vec3& lhs, const Vec3& rhs) {
    return lhs.distance2(rhs);
  }

  float Vec3::dot(const Vec3& lhs, const Vec3& rhs) {
    return lhs.dot(rhs);
  }

  float Vec3::dot4(const Vec3& v4, const Vec3& v3) {
    return v4.dot4(v3);
  }

  Vec3 Vec3::cross(const Vec3& lhs, const Vec3& rhs) {
    return Vec3(lhs.y*rhs.z - lhs.z*rhs.y,
      lhs.z*rhs.x - lhs.x*rhs.z,
      lhs.x*rhs.y - lhs.y*rhs.x);
  }

  int Vec3::leastSignificantAxis(const Vec3& in) {
    return in.leastSignificantAxis();
  }

  int Vec3::mostSignificantAxis(const Vec3& in) {
    return in.mostSignificantAxis();
  }

  Vec3 operator*(float lhs, const Vec3& rhs) {
    return rhs * lhs;
  }

  Vec3& operator*=(float lhs, Vec3& rhs) {
    return rhs *= lhs;
  }

  size_t Vec3Hash::operator()(const Vec3& val) {
    float scalar = 1.0f/SYX_EPSILON;
    int x = static_cast<int>(std::ceil(val.x * scalar));
    int y = static_cast<int>(std::ceil(val.y * scalar));
    int z = static_cast<int>(std::ceil(val.z * scalar));
    //Hash suggested by Optimized Spatial Hashing for Collision Detection of Deformable Objects
    return (x * 73856093 ^ y * 19349663 ^ z * 83492791);
  }
}