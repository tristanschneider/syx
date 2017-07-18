#include "Precompile.h"

namespace Syx {
  const Matrix2 Matrix2::sZero(0.0f, 0.0f,
    0.0f, 0.0f);
  const Matrix2 Matrix2::sIdentity(1.0f, 0.0f,
    0.0f, 1.0f);

  Matrix2 Matrix2::operator+(const Matrix2& rhs) const {
    return Matrix2(mbx + rhs.mbx, mby + rhs.mby);
  }

  Matrix2 Matrix2::operator-(const Matrix2& rhs) const {
    return Matrix2(mbx - rhs.mbx, mby - rhs.mby);
  }

  Matrix2 Matrix2::operator*(const Matrix2& rhs) const {
    Matrix2 temp = Transposed();
    return Matrix2(temp.mbx.Dot(rhs.mbx), temp.mbx.Dot(rhs.mby),
      temp.mby.Dot(rhs.mbx), temp.mby.Dot(rhs.mby));
  }

  Vector2 Matrix2::operator*(const Vector2& rhs) const {
    return Vector2(mbx.x*rhs.x + mby.x*rhs.y, mbx.y*rhs.x + mby.y*rhs.y);
  }

  bool Matrix2::operator==(const Matrix2& rhs) const {
    return mbx == rhs.mbx && mby == rhs.mby;
  }

  bool Matrix2::operator!=(const Matrix2& rhs) const {
    return !(*this == rhs);
  }

  Matrix2& Matrix2::operator+=(const Matrix2& rhs) {
    mbx += rhs.mbx;
    mby += rhs.mby;
    return *this;
  }

  Matrix2& Matrix2::operator-=(const Matrix2& rhs) {
    mbx -= rhs.mbx;
    mby -= rhs.mby;
    return *this;
  }

  Matrix2& Matrix2::operator*=(const Matrix2& rhs) {
    return *this = *this*rhs;
  }

  Matrix2 Matrix2::Rotate(float cosAngle, float sinAngle) {
    return Matrix2(cosAngle, -sinAngle,
      sinAngle, cosAngle);
  }

  Matrix2 Matrix2::Rotate(float ccwRadians) {
    float cosAngle = cos(ccwRadians);
    float sinAngle = sin(ccwRadians);
    return Rotate(cosAngle, sinAngle);
  }

  Matrix2 Matrix2::Rotate(const Vector2& from, const Vector2& to) {
    float sinAngle = from.Cross(to);
    float cosAngle = from.Dot(to);
    return Rotate(cosAngle, sinAngle);
  }

  //[x]   [0]
  //[y] X [0]
  //[0]   [1]
  Matrix2 Matrix2::RotationFromUp(const Vector2& up) {
    //up cross UnitZ
    return Matrix2(up.y, up.x,
      -up.x, up.y);
  }

  //[0]   [x]
  //[0] X [y]
  //[1]   [0]
  Matrix2 Matrix2::RotationFromRight(const Vector2& right) {
    //UnitZ cross right
    return Matrix2(right.x, -right.y,
      right.y, right.x);
  }

  Matrix2 Matrix2::Scale(const Vector2& scale) {
    return Scale(scale.x, scale.y);
  }

  Matrix2 Matrix2::Scale(float scaleX, float scaleY) {
    return Matrix2(scaleX, 0.0f,
      0.0f, scaleY);
  }

  Matrix2 Matrix2::Transposed(void) const {
    return Matrix2(mbx.x, mbx.y,
      mby.x, mby.y);
  }

  Matrix2 Matrix2::TransposedMultiply(const Matrix2& rhs) const {
    return Matrix2(mbx.Dot(rhs.mbx), mbx.Dot(rhs.mby),
      mby.Dot(rhs.mbx), mby.Dot(rhs.mby));
  }

  Vector2 Matrix2::TransposedMultiply(const Vector2& rhs) const {
    return Vector2(mbx.Dot(rhs), mby.Dot(rhs));
  }

  Matrix2 operator*(float scalar, const Matrix2& mat) {
    return Matrix2(mat.mbx.Scale(scalar), mat.mby.Scale(scalar));
  }
}