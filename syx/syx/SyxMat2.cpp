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
    Matrix2 temp = transposed();
    return Matrix2(temp.mbx.dot(rhs.mbx), temp.mbx.dot(rhs.mby),
      temp.mby.dot(rhs.mbx), temp.mby.dot(rhs.mby));
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

  Matrix2 Matrix2::rotate(float cosAngle, float sinAngle) {
    return Matrix2(cosAngle, -sinAngle,
      sinAngle, cosAngle);
  }

  Matrix2 Matrix2::rotate(float ccwRadians) {
    float cosAngle = cos(ccwRadians);
    float sinAngle = sin(ccwRadians);
    return rotate(cosAngle, sinAngle);
  }

  Matrix2 Matrix2::rotate(const Vector2& from, const Vector2& to) {
    float sinAngle = from.cross(to);
    float cosAngle = from.dot(to);
    return rotate(cosAngle, sinAngle);
  }

  //[x]   [0]
  //[y] X [0]
  //[0]   [1]
  Matrix2 Matrix2::rotationFromUp(const Vector2& up) {
    //up cross UnitZ
    return Matrix2(up.y, up.x,
                  -up.x, up.y);
  }

  //[0]   [x]
  //[0] X [y]
  //[1]   [0]
  Matrix2 Matrix2::rotationFromRight(const Vector2& right) {
    //UnitZ cross right
    return Matrix2(right.x, -right.y,
                   right.y, right.x);
  }

  Matrix2 Matrix2::scale(const Vector2& s) {
    return scale(s.x, s.y);
  }

  Matrix2 Matrix2::scale(float scaleX, float scaleY) {
    return Matrix2(scaleX, 0.0f,
                     0.0f, scaleY);
  }

  Matrix2 Matrix2::transposed(void) const {
    return Matrix2(mbx.x, mbx.y,
                   mby.x, mby.y);
  }

  Matrix2 Matrix2::transposedMultiply(const Matrix2& rhs) const {
    return Matrix2(mbx.dot(rhs.mbx), mbx.dot(rhs.mby),
                   mby.dot(rhs.mbx), mby.dot(rhs.mby));
  }

  Vector2 Matrix2::transposedMultiply(const Vector2& rhs) const {
    return Vector2(mbx.dot(rhs), mby.dot(rhs));
  }

  Matrix2 operator*(float scalar, const Matrix2& mat) {
    return Matrix2(mat.mbx.scale(scalar), mat.mby.scale(scalar));
  }
}