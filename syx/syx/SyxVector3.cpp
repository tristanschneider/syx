#include "Precompile.h"
#include "SyxVector3.h"

namespace Syx {
  SAlign const Vector3 Vector3::UnitX(1.0f, 0.0f, 0.0f);
  SAlign const Vector3 Vector3::UnitY(0.0f, 1.0f, 0.0f);
  SAlign const Vector3 Vector3::UnitZ(0.0f, 0.0f, 1.0f);
  SAlign const Vector3 Vector3::Zero(0.0f);
  SAlign const Vector3 Vector3::Identity(1.0f);

  Vector3 Vector3::operator+(const Vector3& rhs) const {
    return Vector3(x + rhs.x, y + rhs.y, z + rhs.z);
  }

  Vector3 Vector3::operator-(const Vector3& rhs) const {
    return Vector3(x - rhs.x, y - rhs.y, z - rhs.z);
  }

  Vector3 Vector3::operator-(void) const {
    return Vector3(-x, -y, -z);
  }

  Vector3 Vector3::operator*(float rhs) const {
    return Vector3(x*rhs, y*rhs, z*rhs);
  }

  Vector3 Vector3::operator/(float rhs) const {
    float divisor = 1.0f/rhs;
    return divisor**this;
  }

  const float& Vector3::operator[](int index) const {
    return *(&x + index);
  }

  float& Vector3::operator[](int index) {
    return *(&x + index);
  }

  Vector3& Vector3::operator+=(const Vector3& rhs) {
    x += rhs.x;
    y += rhs.y;
    z += rhs.z;
    return *this;
  }

  Vector3& Vector3::operator-=(const Vector3& rhs) {
    x -= rhs.x;
    y -= rhs.y;
    z -= rhs.z;
    return *this;
  }

  Vector3& Vector3::operator*=(float rhs) {
    x *= rhs;
    y *= rhs;
    z *= rhs;
    return *this;
  }

  Vector3& Vector3::operator/=(float rhs) {
    float divisor = 1.0f/rhs;
    return *this *= divisor;
  }

  bool Vector3::operator==(const Vector3& rhs) const {
    return Equal(rhs, SYX_EPSILON);
  }

  bool Vector3::operator!=(const Vector3& rhs) const {
    return !Equal(rhs, SYX_EPSILON);
  }

  bool Vector3::Equal(const Vector3& rhs, float epsilon) const {
    return abs(x - rhs.x) <= epsilon &&
      abs(y - rhs.y) <= epsilon &&
      abs(z - rhs.z) <= epsilon;
  }

  //From Erin Catto's box2d forum. Assumes normalized this
  Vector3 Vector3::GetOrthogonal() const {
    //Suppose vector a has all equal components and is a unit vector: a = (s, s, s)
    //Then 3*s*s = 1, s = sqrt(1/3) = 0.57735. This means that at least one component of a
    //unit vector must be greater or equal to 0.57735.
    return std::abs(x) >= 0.57735f ? Vec3(y, -x, 0.0f).Normalized() : Vec3(0.0f, z, -y).Normalized();
  }

  //Assumes normalized this
  void Vector3::GetBasis(Vector3& resultA, Vector3& resultB) const {
    resultA = GetOrthogonal();
    resultB = Cross(resultA);
  }

  float Vector3::Length(void) const {
    return sqrt(Length2());
  }

  float Vector3::Length2(void) const {
    return Dot(*this);
  }

  float Vector3::Distance(const Vector3& other) const {
    return sqrt(Distance2(other));
  }

  float Vector3::Distance2(const Vector3& other) const {
    return (*this - other).Length2();
  }

  float Vector3::Dot(const Vector3& other) const {
    return x*other.x + y*other.y + z*other.z;
  }

  float Vector3::Dot4(const Vector3& other) const {
    return Dot(other) + w;
  }

  Vector3 Vector3::Cross(const Vector3& other) const {
    return Vector3(y*other.z - z*other.y,
      z*other.x - x*other.z,
      x*other.y - y*other.x);
  }

  //Returns index of least significant, so x = 0, y = 1, etc.
  int Vector3::LeastSignificantAxis(void) const {
    Vector3 absIn = Abs(*this);
    if(absIn.x < absIn.y) {
      if(absIn.x < absIn.z)
        return 0; // X < Y < Z, X < Z < Y
      return 2; // Z < X < Y
    }
    else if(absIn.y < absIn.z)
      return 1; // Y < X < Z, Y < Z < X
    return 2; // Z < Y < X
  }

  int Vector3::MostSignificantAxis(void) const {
    Vector3 absIn = Abs(*this);
    if(absIn.x > absIn.y) {
      if(absIn.x > absIn.z)
        return 0; // X > Y > Z, X > Z > Y
      return 2; // Z > X > Y
    }
    else if(absIn.y > absIn.z)
      return 1; // Y > X > Z, Y > Z > X
    return 2; // Z > Y > X
  }

  Vector3 Vector3::Normalized(void) const {
    return *this/Length();
  }

  Vector3 Vector3::SafeNormalized(void) const {
    return SafeDivide(*this, Length());
  }

  void Vector3::SafeDivide(float rhs) {
    float scalar = 0.0f;
    if(abs(rhs) > SYX_EPSILON)
      scalar = 1.0f/rhs;
    *this *= scalar;
  }

  void Vector3::Scale(const Vector3& rhs) {
    x *= rhs.x;
    y *= rhs.y;
    z *= rhs.z;
  }

  void Vector3::Lerp(const Vector3& end, float t) {
    *this = *this + t*(end - *this);
  }

  void Vector3::Abs(void) {
    x = abs(x);
    y = abs(y);
    z = abs(z);
  }

  void Vector3::ProjVec(const Vector3& onto) {
    *this = onto*(Dot(onto)/onto.Length2());
  }

  void Vector3::PointPlaneProj(const Vector3& normal, const Vector3& onPlane) {
    Vector3 proj = ProjVec(*this - onPlane, normal);
    *this -= proj;
  }

  void Vector3::Normalize(void) {
    *this /= Length();
  }

  void Vector3::SafeNormalize(void) {
    SafeDivide(Length());
  }

  Vector3 Vector3::Reciprocal(void) const {
    return Vector3(Syx::SafeDivide(1.0f, x, SYX_EPSILON), Syx::SafeDivide(1.0f, y, SYX_EPSILON), Syx::SafeDivide(1.0f, z, SYX_EPSILON));
  }

  Vector3 Vector3::Reciprocal4(void) const {
    return Vector3(Syx::SafeDivide(1.0f, x, SYX_EPSILON),
      Syx::SafeDivide(1.0f, y, SYX_EPSILON),
      Syx::SafeDivide(1.0f, z, SYX_EPSILON),
      Syx::SafeDivide(1.0f, w, SYX_EPSILON));
  }

  Vector3 Vector3::Mat2Inversed() const {
    //  1   [w ,-y]
    //xw-yz [-z, x]
    float invdet = Syx::SafeDivide(1.0f, x*w - y*z, SYX_EPSILON);
    return Vec3(w*invdet, -y*invdet,
                -z*invdet, x*invdet);
  }

  Vector3 Vector3::Mat2Mul(const Vector3& v) const {
    //[x y]*[x]
    //[z w] [y]
    return Vec3(x*v.x + y*v.y, z*v.x + w*v.y, 0.0f);
  }

  Vector3 Vector3::Mat2MatMul(const Vector3& mat) const {
    //[x y]*[x y]
    //[z w] [z w]
    return Vec3(x*mat.x + y*mat.z, x*mat.y + y*mat.w,
                z*mat.x + w*mat.z, z*mat.y + w*mat.w);
  }

  Vector3 Vector3::SafeDivide(const Vector3& vec, float div) {
    float scalar = 0.0f;
    if(abs(div) > SYX_EPSILON)
      scalar = 1.0f/div;
    return scalar*vec;
  }

  Vector3 Vector3::Scale(const Vector3& lhs, const Vector3& rhs) {
    return Vector3(lhs.x*rhs.x, lhs.y*rhs.y, lhs.z*rhs.z);
  }

  Vector3 Vector3::Lerp(const Vector3& start, const Vector3& end, float t) {
    return start + t*(end - start);
  }

  Vector3 Vector3::Abs(const Vector3& in) {
    return Vector3(abs(in.x), abs(in.y), abs(in.z));
  }

  float Vector3::PointLineDistanceSQ(const Vector3& point, const Vector3& start, const Vector3& end) {
    Vector3 projPoint = start + ProjVec(point - start, end - start);
    return Length2(point - projPoint);
  }

  Vector3 Vector3::CCWTriangleNormal(const Vector3& a, const Vector3& b, const Vector3& c) {
    return (b - a).Cross(c - a);
  }

  Vector3 Vector3::PerpendicularLineToPoint(const Vector3& line, const Vector3& lineToPoint) {
    return Cross(Cross(line, lineToPoint), line);
  }

  Vector3 Vector3::BarycentricToPoint(const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& barycentric) {
    return a*barycentric.x + b*barycentric.y + c*barycentric.z;
  }

  Vector3 Vector3::ProjVec(const Vector3& vec, const Vector3& onto) {
    return onto*(Dot(vec, onto)/Length2(onto));
  }

  float Vector3::ProjVecScalar(const Vector3& vec, const Vector3& onto) {
    return Dot(vec, onto)/Dot(onto, onto);
  }

  Vector3 Vector3::PointPlaneProj(const Vector3& point, const Vector3& normal, const Vector3& onPlane) {
    return point - ProjVec(point - onPlane, normal);
  }

  float Vector3::GetScalarT(const Vector3& start, const Vector3& end, const Vector3& pointOnLine) {
    Vector3 startToEnd = end - start;
    unsigned nonZeroAxis = 0;
    for(unsigned i = 0; i < 3; ++i)
      if(abs(startToEnd[i]) > SYX_EPSILON) {
        nonZeroAxis = i;
        break;
      }

    return (pointOnLine[nonZeroAxis] - start[nonZeroAxis]) / startToEnd[nonZeroAxis];
  }

  float Vector3::Length(const Vector3& in) {
    return in.Length();
  }

  float Vector3::Length2(const Vector3& in) {
    return in.Length2();
  }

  float Vector3::Distance(const Vector3& lhs, const Vector3& rhs) {
    return lhs.Distance(rhs);
  }

  float Vector3::Distance2(const Vector3& lhs, const Vector3& rhs) {
    return lhs.Distance2(rhs);
  }

  float Vector3::Dot(const Vector3& lhs, const Vector3& rhs) {
    return lhs.Dot(rhs);
  }

  float Vector3::Dot4(const Vector3& v4, const Vector3& v3) {
    return v4.Dot4(v3);
  }

  Vector3 Vector3::Cross(const Vector3& lhs, const Vector3& rhs) {
    return Vector3(lhs.y*rhs.z - lhs.z*rhs.y,
      lhs.z*rhs.x - lhs.x*rhs.z,
      lhs.x*rhs.y - lhs.y*rhs.x);
  }

  int Vector3::LeastSignificantAxis(const Vector3& in) {
    return in.LeastSignificantAxis();
  }

  int Vector3::MostSignificantAxis(const Vector3& in) {
    return in.MostSignificantAxis();
  }

  Vector3 operator*(float lhs, const Vector3& rhs) {
    return rhs * lhs;
  }

  Vector3& operator*=(float lhs, Vector3& rhs) {
    return rhs *= lhs;
  }

  size_t Vector3Hash::operator()(const Vector3& val) {
    float scalar = 1.0f/SYX_EPSILON;
    int x = static_cast<int>(std::ceil(val.x * scalar));
    int y = static_cast<int>(std::ceil(val.y * scalar));
    int z = static_cast<int>(std::ceil(val.z * scalar));
    //Hash suggested by Optimized Spatial Hashing for Collision Detection of Deformable Objects
    return (x * 73856093 ^ y * 19349663 ^ z * 83492791);
  }
}