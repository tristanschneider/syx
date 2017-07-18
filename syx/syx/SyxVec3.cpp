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
    return Equal(rhs, SYX_EPSILON);
  }

  bool Vec3::operator!=(const Vec3& rhs) const {
    return !Equal(rhs, SYX_EPSILON);
  }

  bool Vec3::Equal(const Vec3& rhs, float epsilon) const {
    return abs(x - rhs.x) <= epsilon &&
      abs(y - rhs.y) <= epsilon &&
      abs(z - rhs.z) <= epsilon;
  }

  //From Erin Catto's box2d forum. Assumes normalized this
  Vec3 Vec3::GetOrthogonal() const {
    //Suppose vector a has all equal components and is a unit vector: a = (s, s, s)
    //Then 3*s*s = 1, s = sqrt(1/3) = 0.57735. This means that at least one component of a
    //unit vector must be greater or equal to 0.57735.
    return std::abs(x) >= 0.57735f ? Vec3(y, -x, 0.0f).Normalized() : Vec3(0.0f, z, -y).Normalized();
  }

  //Assumes normalized this
  void Vec3::GetBasis(Vec3& resultA, Vec3& resultB) const {
    resultA = GetOrthogonal();
    resultB = Cross(resultA);
  }

  float Vec3::Length(void) const {
    return sqrt(Length2());
  }

  float Vec3::Length2(void) const {
    return Dot(*this);
  }

  float Vec3::Distance(const Vec3& other) const {
    return sqrt(Distance2(other));
  }

  float Vec3::Distance2(const Vec3& other) const {
    return (*this - other).Length2();
  }

  float Vec3::Dot(const Vec3& other) const {
    return x*other.x + y*other.y + z*other.z;
  }

  float Vec3::Dot4(const Vec3& other) const {
    return Dot(other) + w;
  }

  Vec3 Vec3::Cross(const Vec3& other) const {
    return Vec3(y*other.z - z*other.y,
      z*other.x - x*other.z,
      x*other.y - y*other.x);
  }

  //Returns index of least significant, so x = 0, y = 1, etc.
  int Vec3::LeastSignificantAxis(void) const {
    Vec3 absIn = Abs(*this);
    if(absIn.x < absIn.y) {
      if(absIn.x < absIn.z)
        return 0; // X < Y < Z, X < Z < Y
      return 2; // Z < X < Y
    }
    else if(absIn.y < absIn.z)
      return 1; // Y < X < Z, Y < Z < X
    return 2; // Z < Y < X
  }

  int Vec3::MostSignificantAxis(void) const {
    Vec3 absIn = Abs(*this);
    if(absIn.x > absIn.y) {
      if(absIn.x > absIn.z)
        return 0; // X > Y > Z, X > Z > Y
      return 2; // Z > X > Y
    }
    else if(absIn.y > absIn.z)
      return 1; // Y > X > Z, Y > Z > X
    return 2; // Z > Y > X
  }

  Vec3 Vec3::Normalized(void) const {
    return *this/Length();
  }

  Vec3 Vec3::SafeNormalized(void) const {
    return SafeDivide(*this, Length());
  }

  void Vec3::SafeDivide(float rhs) {
    float scalar = 0.0f;
    if(abs(rhs) > SYX_EPSILON)
      scalar = 1.0f/rhs;
    *this *= scalar;
  }

  void Vec3::Scale(const Vec3& rhs) {
    x *= rhs.x;
    y *= rhs.y;
    z *= rhs.z;
  }

  void Vec3::Lerp(const Vec3& end, float t) {
    *this = *this + t*(end - *this);
  }

  void Vec3::Abs(void) {
    x = abs(x);
    y = abs(y);
    z = abs(z);
  }

  void Vec3::ProjVec(const Vec3& onto) {
    *this = onto*(Dot(onto)/onto.Length2());
  }

  void Vec3::PointPlaneProj(const Vec3& normal, const Vec3& onPlane) {
    Vec3 proj = ProjVec(*this - onPlane, normal);
    *this -= proj;
  }

  void Vec3::Normalize(void) {
    *this /= Length();
  }

  void Vec3::SafeNormalize(void) {
    SafeDivide(Length());
  }

  Vec3 Vec3::Reciprocal(void) const {
    return Vec3(Syx::SafeDivide(1.0f, x, SYX_EPSILON), Syx::SafeDivide(1.0f, y, SYX_EPSILON), Syx::SafeDivide(1.0f, z, SYX_EPSILON));
  }

  Vec3 Vec3::Reciprocal4(void) const {
    return Vec3(Syx::SafeDivide(1.0f, x, SYX_EPSILON),
      Syx::SafeDivide(1.0f, y, SYX_EPSILON),
      Syx::SafeDivide(1.0f, z, SYX_EPSILON),
      Syx::SafeDivide(1.0f, w, SYX_EPSILON));
  }

  Vec3 Vec3::Mat2Inversed() const {
    //  1   [w ,-y]
    //xw-yz [-z, x]
    float invdet = Syx::SafeDivide(1.0f, x*w - y*z, SYX_EPSILON);
    return Vec3(w*invdet, -y*invdet,
                -z*invdet, x*invdet);
  }

  Vec3 Vec3::Mat2Mul(const Vec3& v) const {
    //[x y]*[x]
    //[z w] [y]
    return Vec3(x*v.x + y*v.y, z*v.x + w*v.y, 0.0f);
  }

  Vec3 Vec3::Mat2MatMul(const Vec3& mat) const {
    //[x y]*[x y]
    //[z w] [z w]
    return Vec3(x*mat.x + y*mat.z, x*mat.y + y*mat.w,
                z*mat.x + w*mat.z, z*mat.y + w*mat.w);
  }

  Vec3 Vec3::SafeDivide(const Vec3& vec, float div) {
    float scalar = 0.0f;
    if(abs(div) > SYX_EPSILON)
      scalar = 1.0f/div;
    return scalar*vec;
  }

  Vec3 Vec3::Scale(const Vec3& lhs, const Vec3& rhs) {
    return Vec3(lhs.x*rhs.x, lhs.y*rhs.y, lhs.z*rhs.z);
  }

  Vec3 Vec3::Lerp(const Vec3& start, const Vec3& end, float t) {
    return start + t*(end - start);
  }

  Vec3 Vec3::Abs(const Vec3& in) {
    return Vec3(abs(in.x), abs(in.y), abs(in.z));
  }

  float Vec3::PointLineDistanceSQ(const Vec3& point, const Vec3& start, const Vec3& end) {
    Vec3 projPoint = start + ProjVec(point - start, end - start);
    return Length2(point - projPoint);
  }

  Vec3 Vec3::CCWTriangleNormal(const Vec3& a, const Vec3& b, const Vec3& c) {
    return (b - a).Cross(c - a);
  }

  Vec3 Vec3::PerpendicularLineToPoint(const Vec3& line, const Vec3& lineToPoint) {
    return Cross(Cross(line, lineToPoint), line);
  }

  Vec3 Vec3::BarycentricToPoint(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& barycentric) {
    return a*barycentric.x + b*barycentric.y + c*barycentric.z;
  }

  Vec3 Vec3::ProjVec(const Vec3& vec, const Vec3& onto) {
    return onto*(Dot(vec, onto)/Length2(onto));
  }

  float Vec3::ProjVecScalar(const Vec3& vec, const Vec3& onto) {
    return Dot(vec, onto)/Dot(onto, onto);
  }

  Vec3 Vec3::PointPlaneProj(const Vec3& point, const Vec3& normal, const Vec3& onPlane) {
    return point - ProjVec(point - onPlane, normal);
  }

  float Vec3::GetScalarT(const Vec3& start, const Vec3& end, const Vec3& pointOnLine) {
    Vec3 startToEnd = end - start;
    unsigned nonZeroAxis = 0;
    for(unsigned i = 0; i < 3; ++i)
      if(abs(startToEnd[i]) > SYX_EPSILON) {
        nonZeroAxis = i;
        break;
      }

    return (pointOnLine[nonZeroAxis] - start[nonZeroAxis]) / startToEnd[nonZeroAxis];
  }

  float Vec3::Length(const Vec3& in) {
    return in.Length();
  }

  float Vec3::Length2(const Vec3& in) {
    return in.Length2();
  }

  float Vec3::Distance(const Vec3& lhs, const Vec3& rhs) {
    return lhs.Distance(rhs);
  }

  float Vec3::Distance2(const Vec3& lhs, const Vec3& rhs) {
    return lhs.Distance2(rhs);
  }

  float Vec3::Dot(const Vec3& lhs, const Vec3& rhs) {
    return lhs.Dot(rhs);
  }

  float Vec3::Dot4(const Vec3& v4, const Vec3& v3) {
    return v4.Dot4(v3);
  }

  Vec3 Vec3::Cross(const Vec3& lhs, const Vec3& rhs) {
    return Vec3(lhs.y*rhs.z - lhs.z*rhs.y,
      lhs.z*rhs.x - lhs.x*rhs.z,
      lhs.x*rhs.y - lhs.y*rhs.x);
  }

  int Vec3::LeastSignificantAxis(const Vec3& in) {
    return in.LeastSignificantAxis();
  }

  int Vec3::MostSignificantAxis(const Vec3& in) {
    return in.MostSignificantAxis();
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