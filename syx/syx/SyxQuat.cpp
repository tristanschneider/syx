#include "Precompile.h"

namespace Syx {
  const Quat Quat::Zero(0.0f, 0.0f, 0.0f, 0.0f);
  const Quat Quat::Identity(0.0f, 0.0f, 0.0f, 1.0f);

  Quat::Quat(const Vec3& v, float w): mV(v, w) {}

  Quat::Quat(float i, float j, float k, float w) : mV(i, j, k, w) {}

  Quat Quat::operator*(const Quat& rhs) const {
    return Quat(mV.w*rhs.mV + rhs.mV.w*mV + mV.cross(rhs.mV), mV.w*rhs.mV.w - mV.dot(rhs.mV));
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
    Quat temp = Quat(mV.w*rhs + mV.cross(rhs), -mV.dot(rhs));
    Quat neg = inversed();
    return temp.mV.w*neg.mV + neg.mV.w*temp.mV + temp.mV.cross(neg.mV);
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

  float Quat::length(void) const {
    return sqrt(length2());
  }

  float Quat::length2(void) const {
    return mV.length2() + mV.w*mV.w;
  }

  Quat Quat::normalized(void) const {
    return *this / length();
  }

  void Quat::normalize(void) {
    *this /= length();
  }

  Quat Quat::inversed(void) const {
    return Quat(-mV, mV.w);
  }

  void Quat::inverse(void) {
    *this = Quat(-mV, mV.w);
  }

  Mat3 Quat::toMatrix(void) const {
    return Mat3(1.0f - 2.0f*mV.y*mV.y - 2.0f*mV.z*mV.z, 2.0f*mV.x*mV.y - 2.0f*mV.z*mV.w, 2.0f*mV.x*mV.z + 2.0f*mV.y*mV.w,
      2.0f*mV.x*mV.y + 2.0f*mV.z*mV.w, 1.0f - 2.0f*mV.x*mV.x - 2.0f*mV.z*mV.z, 2.0f*mV.y*mV.z - 2.0f*mV.x*mV.w,
      2.0f*mV.x*mV.z - 2.0f*mV.y*mV.w, 2.0f*mV.y*mV.z + 2.0f*mV.x*mV.w, 1.0f - 2.0f*mV.x*mV.x - 2.0f*mV.y*mV.y);
  }

  Quat Quat::axisAngle(const Vec3& axis, float angle) {
    float angle2 = 0.5f*angle;
    return Quat(axis*sin(angle2), cos(angle2));
  }

  Quat Quat::lookAt(const Vec3& axis) {
    Vec3 up = std::abs(axis.y - 1.0f) < SYX_EPSILON ? Vec3::UnitZ : Vec3::UnitY;
    Vec3 right = up.cross(axis).normalized();
    up = axis.cross(right);
    return lookAt(axis, up, right);
  }

  Quat Quat::lookAt(const Vec3& axis, const Vec3& up) {
    return lookAt(axis, up, up.cross(axis));
  }

  Quat Quat::lookAt(const Vec3& forward, const Vec3& up, const Vec3& right) {
    SyxAssertError(right.cross(up).dot(forward) > 0.0f);
    //There's probably a better way with quaternion math, but whatever, this is easier.
    return Mat3(right, up, forward).toQuat();
  }

  Quat Quat::getRotation(const Vec3& from, const Vec3& to) {
    Vec3 axis = from.cross(to);
    float cosAngle = from.dot(to);

    //If angle is 180 pick an arbitrary axis orthogonal to from
    if(cosAngle <= -1.0f + SYX_EPSILON) {
      axis = from.getOrthogonal();
      return axisAngle(axis, SYX_PI);
    }

    //Game Programming Gems 2.10 sin cos optimization madness
    float s = std::sqrt((1.0f + cosAngle)*2.0f);
    float rs = 1.0f/s;
    return Quat(axis*rs, s*0.5f);
  }

  Quat operator*(float lhs, const Quat& rhs) {
    return rhs * lhs;
  }

  Vec3 Quat::getUp(void) const {
    return *this * Vec3::UnitY;
  }

  Vec3 Quat::getRight(void) const {
    return *this * Vec3::UnitX;
  }

  Vec3 Quat::getForward(void) const {
    return *this * Vec3::UnitZ;
  }

  float Quat::getAngle() const {
    return 2.0f*std::acos(mV.w);
  }

  Vec3 Quat::getAxis() const {
    return Vec3(mV.x, mV.y, mV.z);
  }
}