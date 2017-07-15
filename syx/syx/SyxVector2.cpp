#include "SyxVector2.h"
#include "SyxMath.h"

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
    return Vector2(SafeDivide(x, denom, SYX_EPSILON), SafeDivide(y, denom, SYX_EPSILON));
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

  float Vector2::Dot(const Vector2& rhs) const {
    return x*rhs.x + y*rhs.y;
  }

  float Vector2::Cross(const Vector2& rhs) const {
    return x*rhs.y - y*rhs.x;
  }

  Vector2 Vector2::Proj(const Vector2& onto) const {
    return onto*ProjScalar(onto);
  }

  float Vector2::ProjScalar(const Vector2& onto) const {
    return SafeDivide(Dot(onto), onto.Dot(onto), SYX_EPSILON);
  }

  Vector2 Vector2::Scale(const Vector2& scalar) const {
    return Vector2(x*scalar.x, y*scalar.y);
  }

  float Vector2::Length(void) const {
    return sqrt(Length2());
  }

  float Vector2::Length2(void) const {
    return Dot(*this);
  }

  float Vector2::Dist(const Vector2& to) const {
    return sqrt(Dist2(to));
  }

  float Vector2::Dist2(const Vector2& to) const {
    return (*this - to).Length2();
  }

  float Vector2::Normalize(void) {
    float length = Length();
    *this /= length;
    return length;
  }

  Vector2 Vector2::Normalized(void) const {
    return *this/Length();
  }

  Vector2 Vector2::Lerp(const Vector2& to, float t) const {
    return *this + t*(to - *this);
  }

  Vector2 Vector2::Slerp(const Vector2& to, float t) const {
    //Could use dot for cosAngle and manipulate them both, but it's easier to work with the radian angle
    //this would also save the cos and sin calls in rotate
    float sinAngle = Cross(to);
    float cosAngle = Dot(to);
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
    return Rotate(angleDiff*t);
  }

  Vector2 Vector2::Rotate(float ccwRadians) const {
    return Matrix2::Rotate(ccwRadians)**this;
  }

  Vector2 operator*(float lhs, const Vector2& rhs) {
    return rhs*lhs;
  }

  bool Vector2::Equal(const Vector2& rhs, float epsilon) {
    return abs(x - rhs.x) < epsilon && abs(y - rhs.y) < epsilon;
  }

}