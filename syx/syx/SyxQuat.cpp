#include "Precompile.h"

namespace Syx {
  const Quat Quat::Zero(0.0f, 0.0f, 0.0f, 0.0f);
  const Quat Quat::Identity(0.0f, 0.0f, 0.0f, 1.0f);

  Quat::Quat(const Vec3& v, float w): mV(v, w) {}

  Quat::Quat(float i, float j, float k, float w) : mV(i, j, k, w) {}

  Quat Quat::operator*(const Quat& rhs) const {
    return Quat(mV.w*rhs.mV + rhs.mV.w*mV + mV.Cross(rhs.mV), mV.w*rhs.mV.w - mV.Dot(rhs.mV));
  }

  Quat& Quat::operator*=(const Quat& rhs) {
    return *this = *this * rhs;
  }

  Quat Quat::operator*(float rhs) const {
    return Quat(mV*rhs, mV.w*rhs);
  }

  Quat& Quat::operator*=(float rhs) {
    return *this = *this * rhs;
  }

  Vec3 Quat::operator*(const Vec3& rhs) const {
    Quat temp = Quat(mV.w*rhs + mV.Cross(rhs), -mV.Dot(rhs));
    Quat neg = Inversed();
    return temp.mV.w*neg.mV + neg.mV.w*temp.mV + temp.mV.Cross(neg.mV);
  }

  Quat Quat::operator+(const Quat& rhs) const {
    return Quat(mV + rhs.mV, mV.w + rhs.mV.w);
  }

  Quat& Quat::operator+=(const Quat& rhs) {
    return *this = *this + rhs;
  }

  Quat Quat::operator/(float rhs) const {
    return *this * (1.0f/rhs);
  }

  Quat& Quat::operator/=(float rhs) {
    return *this = *this / rhs;
  }

  Quat Quat::operator-(void) const {
    return Quat(-mV, -mV.w);
  }

  float Quat::Length(void) const {
    return sqrt(Length2());
  }

  float Quat::Length2(void) const {
    return mV.Length2() + mV.w*mV.w;
  }

  Quat Quat::Normalized(void) const {
    return *this / Length();
  }

  void Quat::Normalize(void) {
    *this /= Length();
  }

  Quat Quat::Inversed(void) const {
    return Quat(-mV, mV.w);
  }

  void Quat::Inverse(void) {
    *this = Quat(-mV, mV.w);
  }

  Mat3 Quat::ToMatrix(void) const {
    return Mat3(1.0f - 2.0f*mV.y*mV.y - 2.0f*mV.z*mV.z, 2.0f*mV.x*mV.y - 2.0f*mV.z*mV.w, 2.0f*mV.x*mV.z + 2.0f*mV.y*mV.w,
      2.0f*mV.x*mV.y + 2.0f*mV.z*mV.w, 1.0f - 2.0f*mV.x*mV.x - 2.0f*mV.z*mV.z, 2.0f*mV.y*mV.z - 2.0f*mV.x*mV.w,
      2.0f*mV.x*mV.z - 2.0f*mV.y*mV.w, 2.0f*mV.y*mV.z + 2.0f*mV.x*mV.w, 1.0f - 2.0f*mV.x*mV.x - 2.0f*mV.y*mV.y);
  }

  Quat Quat::AxisAngle(const Vec3& axis, float angle) {
    float angle2 = 0.5f*angle;
    return Quat(axis*sin(angle2), cos(angle2));
  }

  Quat Quat::LookAt(const Vec3& axis) {
    Vec3 up = std::abs(axis.y - 1.0f) < SYX_EPSILON ? Vec3::UnitZ : Vec3::UnitY;
    Vec3 right = up.Cross(axis).Normalized();
    up = axis.Cross(right);
    return LookAt(axis, up, right);
  }

  Quat Quat::LookAt(const Vec3& axis, const Vec3& up) {
    return LookAt(axis, up, up.Cross(axis));
  }

  Quat Quat::LookAt(const Vec3& forward, const Vec3& up, const Vec3& right) {
    SyxAssertError(right.Cross(up).Dot(forward) > 0.0f);
    //There's probably a better way with quaternion math, but whatever, this is easier.
    return Mat3(right, up, forward).ToQuat();
  }

  Quat Quat::GetRotation(const Vec3& from, const Vec3& to) {
    Vec3 axis = from.Cross(to);
    float cosAngle = from.Dot(to);

    //If angle is 180 pick an arbitrary axis orthogonal to from
    if(cosAngle <= -1.0f + SYX_EPSILON) {
      axis = from.GetOrthogonal();
      return AxisAngle(axis, SYX_PI);
    }

    //Game Programming Gems 2.10 sin cos optimization madness
    float s = std::sqrt((1.0f + cosAngle)*2.0f);
    float rs = 1.0f/s;
    return Quat(axis*rs, s*0.5f);
  }

  Quat operator*(float lhs, const Quat& rhs) {
    return rhs * lhs;
  }

  Vec3 Quat::GetUp(void) const {
    return *this * Vec3::UnitY;
  }

  Vec3 Quat::GetRight(void) const {
    return *this * Vec3::UnitX;
  }

  Vec3 Quat::GetForward(void) const {
    return *this * Vec3::UnitZ;
  }

  float Quat::GetAngle() const {
    return 2.0f*std::acos(mV.w);
  }

  Vec3 Quat::GetAxis() const {
    return Vec3(mV.x, mV.y, mV.z);
  }
}