#include "Precompile.h"

namespace Syx {
  const Vector2 Vector2::sUnitX(1.0f, 0.0f);
  const Vector2 Vector2::sUnitY(0.0f, 1.0f);
  const Vector2 Vector2::sZero(0.0f);
  const Vector2 Vector2::sIdentity(1.0f);

  Vector2 Vector2::operator+(const Vector2& rhs) const {
    return Vector2(x + rhs.x, y + rhs.y);
  }

  Vector2 Vector2::operator-(const Vector2& rhs) const {
    return Vector2(x - rhs.x, y - rhs.y);
  }

  Vector2 Vector2::operator*(float scalar) const {
    return Vector2(x*scalar, y*scalar);
  }

  Vector2 Vector2::operator/(float denom) const {
    return Vector2(safeDivide(x, denom, SYX_EPSILON), safeDivide(y, denom, SYX_EPSILON));
  }

  bool Vector2::operator==(const Vector2& rhs) const {
    return abs(x - rhs.x) < SYX_EPSILON && abs(y - rhs.y) < SYX_EPSILON;
  }

  bool Vector2::operator!=(const Vector2& rhs) const {
    return !(*this == rhs);
  }

  Vector2 Vector2::operator-(void) const {
    return Vector2(-x, -y);
  }

  Vector2& Vector2::operator+=(const Vector2& rhs) {
    return *this = *this + rhs;
  }

  Vector2& Vector2::operator-=(const Vector2& rhs) {
    return *this = *this - rhs;
  }

  Vector2& Vector2::operator*=(float scalar) {
    return *this = *this*scalar;
  }

  Vector2& Vector2::operator/=(float denom) {
    return *this = *this/denom;
  }

  float Vector2::dot(const Vector2& rhs) const {
    return x*rhs.x + y*rhs.y;
  }

  float Vector2::cross(const Vector2& rhs) const {
    return x*rhs.y - y*rhs.x;
  }

  Vector2 Vector2::proj(const Vector2& onto) const {
    return onto*projScalar(onto);
  }

  float Vector2::projScalar(const Vector2& onto) const {
    return safeDivide(dot(onto), onto.dot(onto), SYX_EPSILON);
  }

  Vector2 Vector2::scale(const Vector2& scalar) const {
    return Vector2(x*scalar.x, y*scalar.y);
  }

  float Vector2::length() const {
    return sqrt(length2());
  }

  float Vector2::length2(void) const {
    return dot(*this);
  }

  float Vector2::dist(const Vector2& to) const {
    return sqrt(dist2(to));
  }

  float Vector2::dist2(const Vector2& to) const {
    return (*this - to).length2();
  }

  float Vector2::normalize(void) {
    float len = length();
    *this /= len;
    return len;
  }

  Vector2 Vector2::normalized(void) const {
    return *this/length();
  }

  Vector2 Vector2::lerp(const Vector2& to, float t) const {
    return *this + t*(to - *this);
  }

  Vector2 Vector2::slerp(const Vector2& to, float t) const {
    //Could use dot for cosAngle and manipulate them both, but it's easier to work with the radian angle
    //this would also save the cos and sin calls in rotate
    float sinAngle = cross(to);
    float cosAngle = dot(to);
    float angleDiff = asinf(sinAngle);
    //Rotation of more than 90 degrees, add that on, as the asin will only be between -90 to 90
    if(cosAngle < 0.0f) {
      if(angleDiff > 0.0f)
        angleDiff += SYX_PI_2;
      else
        angleDiff -= SYX_PI_2;
    }
    //Vectors are parallel, figure out if they're the same direction or opposite
    if(abs(sinAngle) < SYX_EPSILON) {
      if(cosAngle > 0.0f)
        angleDiff = 0.0f;
      else
        angleDiff = SYX_PI;
    }
    return rotate(angleDiff*t);
  }

  Vector2 Vector2::rotate(float ccwRadians) const {
    return Matrix2::rotate(ccwRadians)**this;
  }

  Vector2 operator*(float lhs, const Vector2& rhs) {
    return rhs*lhs;
  }

  bool Vector2::equal(const Vector2& rhs, float epsilon) {
    return abs(x - rhs.x) < epsilon && abs(y - rhs.y) < epsilon;
  }

}