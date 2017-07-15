#include "Precompile.h"
#include "SyxMatrix3.h"
#include "SyxVector3.h"
#include "SyxMath.h"
#include "SyxQuaternion.h"

namespace Syx {
  const Matrix3 Matrix3::Identity = Matrix3(1.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 1.0f);
  const Matrix3 Matrix3::Zero = Matrix3(0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f);

  Matrix3::Matrix3(float aa, float ab, float ac,
    float ba, float bb, float bc,
    float ca, float cb, float cc):
    mbx(aa, ba, ca),
    mby(ab, bb, cb),
    mbz(ac, bc, cc) {
  }

  Matrix3::Matrix3(const Vector3& bx, const Vector3& by, const Vector3& bz):
    mbx(bx), mby(by), mbz(bz) {
  }

  Matrix3::Matrix3(float a, float b, float c,
                            float d, float e,
                                     float f)
    : mbx(a, b, c)
    , mby(b, d, e)
    , mbz(c, e, f) {}

  Matrix3::Matrix3(const Vector3& diag):
    mbx(diag.x, 0.0f, 0.0f),
    mby(0.0f, diag.y, 0.0f),
    mbz(0.0f, 0.0f, diag.z) {
  }

  Matrix3 Matrix3::Scaled(const Vector3& scale) const {
    //Wastes a bunch of non-diagonal multiplications. Can optimize later if I feel like it
    Matrix3 temp = Matrix3::Identity;
    temp.SetDiagonal(scale);
    return *this * temp;
  }

  void Matrix3::SetDiagonal(float x, float y, float z) {
    mbx.x = x;
    mby.y = y;
    mbz.z = z;
  }

  void Matrix3::SetDiagonal(const Vector3& diag) {
    SetDiagonal(diag.x, diag.y, diag.z);
  }

  Vector3 Matrix3::GetDiagonal(void) const {
    return Vector3(Get(0, 0), Get(1, 1), Get(2, 2));
  }

  void Matrix3::SetRow(int index, const Vector3& row) {
    SetRow(index, row.x, row.y, row.z);
  }

  void Matrix3::SetRow(int index, float x, float y, float z) {
    Get(index, 0) = x;
    Get(index, 1) = y;
    Get(index, 2) = z;
  }

  float& Matrix3::Get(int row, int col) {
    return *(&mbx.x + row + 4*col);
  }

  const float& Matrix3::Get(int row, int col) const {
    return *(&mbx.x + row + 4*col);
  }

  Vector3 Matrix3::GetRow(int index) const {
    return Vector3(Get(index, 0), Get(index, 1), Get(index, 2));
  }

  const Vec3& Matrix3::GetCol(int index) const {
    switch(index) {
      case 0: return mbx;
      case 1: return mby;
      case 2: return mbz;
      default: SyxAssertError(false, "invalid index"); return mbx;
    }
  }

  Vec3& Matrix3::GetCol(int index) {
    switch(index) {
      case 0: return mbx;
      case 1: return mby;
      case 2: return mbz;
      default: SyxAssertError(false, "invalid index"); return mbx;
    }
  }

  Vector3& Matrix3::operator[](int index) {
    return *(&mbx + index);
  }

  const Vector3& Matrix3::operator[](int index) const {
    return *(&mbx + index);
  }

  Matrix3 Matrix3::operator*(const Matrix3& rhs) const {
    return Matrix3(Get(0, 0)*rhs.Get(0, 0) + Get(0, 1)*rhs.Get(1, 0) + Get(0, 2)*rhs.Get(2, 0),
      Get(0, 0)*rhs.Get(0, 1) + Get(0, 1)*rhs.Get(1, 1) + Get(0, 2)*rhs.Get(2, 1),
      Get(0, 0)*rhs.Get(0, 2) + Get(0, 1)*rhs.Get(1, 2) + Get(0, 2)*rhs.Get(2, 2),

      Get(1, 0)*rhs.Get(0, 0) + Get(1, 1)*rhs.Get(1, 0) + Get(1, 2)*rhs.Get(2, 0),
      Get(1, 0)*rhs.Get(0, 1) + Get(1, 1)*rhs.Get(1, 1) + Get(1, 2)*rhs.Get(2, 1),
      Get(1, 0)*rhs.Get(0, 2) + Get(1, 1)*rhs.Get(1, 2) + Get(1, 2)*rhs.Get(2, 2),

      Get(2, 0)*rhs.Get(0, 0) + Get(2, 1)*rhs.Get(1, 0) + Get(2, 2)*rhs.Get(2, 0),
      Get(2, 0)*rhs.Get(0, 1) + Get(2, 1)*rhs.Get(1, 1) + Get(2, 2)*rhs.Get(2, 1),
      Get(2, 0)*rhs.Get(0, 2) + Get(2, 1)*rhs.Get(1, 2) + Get(2, 2)*rhs.Get(2, 2));
  }

  Matrix3& Matrix3::operator*=(const Matrix3& rhs) {
    return *this = *this * rhs;
  }

  Vector3 Matrix3::operator*(const Vector3& rhs) const {
    return Vector3(Get(0, 0)*rhs.x + Get(0, 1)*rhs.y + Get(0, 2)*rhs.z,
      Get(1, 0)*rhs.x + Get(1, 1)*rhs.y + Get(1, 2)*rhs.z,
      Get(2, 0)*rhs.x + Get(2, 1)*rhs.y + Get(2, 2)*rhs.z);
  }

  Matrix3 Matrix3::operator*(float rhs) const {
    return Matrix3(mbx*rhs, mby*rhs, mbz*rhs);
  }

  Matrix3& Matrix3::operator*=(float rhs) {
    mbx *= rhs;
    mby *= rhs;
    mbz *= rhs;
    return *this;
  }

  Matrix3 Matrix3::operator+(const Matrix3& rhs) const {
    return Matrix3(mbx + rhs.mbx, mby + rhs.mby, mbz + rhs.mbz);
  }

  Matrix3& Matrix3::operator+=(const Matrix3& rhs) {
    mbx += rhs.mbx;
    mby += rhs.mby;
    mbz += rhs.mbz;
    return *this;
  }

  Matrix3 Matrix3::operator-(const Matrix3& rhs) const {
    return Matrix3(mbx - rhs.mbx, mby - rhs.mby, mbz - rhs.mbz);
  }

  Matrix3& Matrix3::operator-=(const Matrix3& rhs) {
    mbx -= rhs.mbx;
    mby -= rhs.mby;
    mbz -= rhs.mbz;
    return *this;
  }

  Matrix3 Matrix3::operator/(float rhs) const {
    return *this * (1.0f/rhs);
  }

  Matrix3& Matrix3::operator/=(float rhs) {
    return *this *= (1.0f/rhs);
  }

  Quat Matrix3::ToQuat(void) {
    float trace = Get(0, 0) + Get(1, 1) + Get(2, 2);
    if(trace > SYX_EPSILON) {
      float s = sqrt(trace + 1.0f)*2.0f;
      return Quat((Get(2, 1) - Get(1, 2))/s, (Get(0, 2) - Get(2, 0))/s, (Get(1, 0) - Get(0, 1))/s, 0.25f*s);
    }
    if(Get(0, 0) > Get(1, 1) && Get(0, 0) > Get(2, 2)) {
      float s = sqrt(1.0f + Get(0, 0) - Get(1, 1) - Get(2, 2))*2.0f;
      return Quat(0.25f*s, (Get(0, 1) + Get(1, 0))/s, (Get(0, 2) + Get(2, 0))/s, (Get(2, 1) - Get(1, 2))/s);
    }
    if(Get(1, 1) > Get(2, 2)) {
      float s = sqrt(1.0f + Get(1, 1) - Get(0, 0) - Get(2, 2))*2.0f;
      return Quat((Get(0, 1) + Get(1, 0))/s, 0.25f*s, (Get(1, 2) + Get(2, 1))/s, (Get(0, 2) - Get(2, 0))/s);
    }
    float s = sqrt(1.0f + Get(2, 2) - Get(0, 0) - Get(1, 1))*2.0f;
    return Quat((Get(0, 2) + Get(2, 0))/s, (Get(1, 2) + Get(2, 1))/s, 0.25f*s, (Get(1, 0) - Get(0, 1))/s);
  }

  Matrix3 Matrix3::Transposed(void) const {
    return Matrix3(Get(0, 0), Get(1, 0), Get(2, 0),
      Get(0, 1), Get(1, 1), Get(2, 1),
      Get(0, 2), Get(1, 2), Get(2, 2));
  }

  void Matrix3::Transpose(void) {
    std::swap(Get(0, 1), Get(1, 0));
    std::swap(Get(0, 2), Get(2, 0));
    std::swap(Get(2, 1), Get(1, 2));
  }

  Matrix3 Matrix3::TransposedMultiply(const Matrix3& rhs) const {
    return Matrix3(Get(0, 0)*rhs.Get(0, 0) + Get(1, 0)*rhs.Get(1, 0) + Get(2, 0)*rhs.Get(2, 0),
      Get(0, 0)*rhs.Get(0, 1) + Get(1, 0)*rhs.Get(1, 1) + Get(2, 0)*rhs.Get(2, 1),
      Get(0, 0)*rhs.Get(0, 2) + Get(1, 0)*rhs.Get(1, 2) + Get(2, 0)*rhs.Get(2, 2),

      Get(0, 1)*rhs.Get(0, 0) + Get(1, 1)*rhs.Get(1, 0) + Get(2, 1)*rhs.Get(2, 0),
      Get(0, 1)*rhs.Get(0, 1) + Get(1, 1)*rhs.Get(1, 1) + Get(2, 1)*rhs.Get(2, 1),
      Get(0, 1)*rhs.Get(0, 2) + Get(1, 1)*rhs.Get(1, 2) + Get(2, 1)*rhs.Get(2, 2),

      Get(0, 2)*rhs.Get(0, 0) + Get(1, 2)*rhs.Get(1, 0) + Get(2, 2)*rhs.Get(2, 0),
      Get(0, 2)*rhs.Get(0, 1) + Get(1, 2)*rhs.Get(1, 1) + Get(2, 2)*rhs.Get(2, 1),
      Get(0, 2)*rhs.Get(0, 2) + Get(1, 2)*rhs.Get(1, 2) + Get(2, 2)*rhs.Get(2, 2));
  }

  Vector3 Matrix3::TransposedMultiply(const Vector3& rhs) const {
    return Vector3(Get(0, 0)*rhs.x + Get(1, 0)*rhs.y + Get(2, 0)*rhs.z,
      Get(0, 1)*rhs.x + Get(1, 1)*rhs.y + Get(2, 1)*rhs.z,
      Get(0, 2)*rhs.x + Get(1, 2)*rhs.y + Get(2, 2)*rhs.z);
  }

  Matrix3 Matrix3::Inverse(void) const {
    return Inverse(Determinant());
  }

  Matrix3 Matrix3::Inverse(float det) const {
    float invdet = SafeDivide(1.0f, det, SYX_EPSILON);

    Matrix3 result;
    result.Get(0, 0) = (Get(1, 1) * Get(2, 2) - Get(2, 1) * Get(1, 2))*invdet;
    result.Get(0, 1) = -(Get(0, 1) * Get(2, 2) - Get(0, 2) * Get(2, 1))*invdet;
    result.Get(0, 2) = (Get(0, 1) * Get(1, 2) - Get(0, 2) * Get(1, 1))*invdet;
    result.Get(1, 0) = -(Get(1, 0) * Get(2, 2) - Get(1, 2) * Get(2, 0))*invdet;
    result.Get(1, 1) = (Get(0, 0) * Get(2, 2) - Get(0, 2) * Get(2, 0))*invdet;
    result.Get(1, 2) = -(Get(0, 0) * Get(1, 2) - Get(1, 0) * Get(0, 2))*invdet;
    result.Get(2, 0) = (Get(1, 0) * Get(2, 1) - Get(2, 0) * Get(1, 1))*invdet;
    result.Get(2, 1) = -(Get(0, 0) * Get(2, 1) - Get(2, 0) * Get(0, 1))*invdet;
    result.Get(2, 2) = (Get(0, 0) * Get(1, 1) - Get(1, 0) * Get(0, 1))*invdet;
    return result;
  }

  float Matrix3::Determinant(void) const {
    return Get(0, 0) * (Get(1, 1) * Get(2, 2) - Get(2, 1) * Get(1, 2)) -
      Get(0, 1) * (Get(1, 0) * Get(2, 2) - Get(1, 2) * Get(2, 0)) +
      Get(0, 2) * (Get(1, 0) * Get(2, 1) - Get(1, 1) * Get(2, 0));
  }

  Matrix3 operator*(float lhs, const Matrix3& rhs) {
    return rhs * lhs;
  }

  Matrix3 Matrix3::AxisAngle(const Vector3& axis, float cosAngle, float sinAngle) {
    /* Using Axis-Angle formula:
    xxC + c, xyC - zs, xzC + ys
    yxC + zs, yyC + c, yzC - xs,
    zxC - ys, zyC + xs, zzC + c
    where c is cosAngle
    s is sinAngle
    C is 1 - c
    and xyz are components of the axis to rotate around*/
    float c = cosAngle;
    float s = sinAngle;
    float C = 1.0f - c;
    float x = axis.x;
    float y = axis.y;
    float z = axis.z;

    return Matrix3(x*x*C + c, x*y*C - z*s, x*z*C + y*s,
      y*x*C + z*s, y*y*C + c, y*z*C - x*s,
      z*x*C - y*s, z*y*C + x*s, z*z*C + c);
  }

  Matrix3 Matrix3::AxisAngle(const Vector3& axis, float angle) {
    return AxisAngle(axis, cosf(angle), sinf(angle));
  }

  Matrix3 Matrix3::XRot(float angle) {
    return XRot(sin(angle), cos(angle));
  }

  Matrix3 Matrix3::YRot(float angle) {
    return YRot(sin(angle), cos(angle));
  }

  Matrix3 Matrix3::ZRot(float angle) {
    return ZRot(sin(angle), cos(angle));
  }

  Matrix3 Matrix3::XRot(float sinAngle, float cosAngle) {
    return Matrix3(1.0f, 0.0f, 0.0f,
      0.0f, cosAngle, -sinAngle,
      0.0f, sinAngle, cosAngle);
  }

  Matrix3 Matrix3::YRot(float sinAngle, float cosAngle) {
    return Matrix3(cosAngle, 0.0f, sinAngle,
      0.0f, 1.0f, 0.0f,
      -sinAngle, 0.0f, cosAngle);
  }

  Matrix3 Matrix3::ZRot(float sinAngle, float cosAngle) {
    return Matrix3(cosAngle, -sinAngle, 0.0f,
      sinAngle, cosAngle, 0.0f,
      0.0f, 0.0f, 1.0f);
  }

  Matrix3 Matrix3::OuterProduct(const Vector3& lhs, const Vector3& rhs) {
    return Matrix3(lhs.x*rhs.x, lhs.x*rhs.y, lhs.x*rhs.z,
                   lhs.y*rhs.x, lhs.y*rhs.y, lhs.y*rhs.z,
                   lhs.z*rhs.x, lhs.z*rhs.y, lhs.z*rhs.z);
  }

  Matrix3 Matrix3::OuterProduct(const Vector3& v) {
    float xy = v.x*v.y;
    float xz = v.x*v.z;
    float yz = v.y*v.z;
    return Matrix3(v.x*v.x, xy, xz,
                  xy, v.y*v.y, yz,
                  xz, yz, v.z*v.z);
  }

  //From Bullet
  void Matrix3::Diagonalize(void) {
    Matrix3 orientation = Matrix3::Zero;
    Matrix3& matrix = *this;

    int maxIterations = 100;
    float epsilon = SYX_EPSILON;

    for(int step = maxIterations; step > 0; step--) {
      // find off-diagonal element [p][q] with largest magnitude
      int p = 0;
      int q = 1;
      int r = 2;
      float max = abs(matrix[0][1]);
      float v = abs(matrix[0][2]);
      if(v > max) {
        q = 2;
        r = 1;
        max = v;
      }
      v = abs(matrix[1][2]);
      if(v > max) {
        p = 1;
        q = 2;
        r = 0;
        max = v;
      }

      float t = epsilon * (abs(matrix[0][0]) + abs(matrix[1][1]) + abs(matrix[2][2]));
      if(max <= t) {
        if(max <= SYX_EPSILON * t)
          return;
        step = 1;
      }

      // compute Jacobi rotation J which leads to a zero for element [p][q]
      float mpq = matrix[p][q];
      float theta = (matrix[q][q] - matrix[p][p]) / (2 * mpq);
      float theta2 = theta * theta;
      float cos;
      float sin;
      if(theta2 * theta2 < 10.0f / SYX_EPSILON) {
        t = (theta >= 0) ? 1 / (theta + sqrt(1 + theta2))
          : 1 / (theta - sqrt(1 + theta2));
        cos = 1 / sqrt(1 + t * t);
        sin = cos * t;
      }
      else {
        // approximation for large theta-value, i.e., a nearly diagonal matrix
        t = 1 / (theta * (2 + 0.5f / theta2));
        cos = 1 - 0.5f * t * t;
        sin = cos * t;
      }

      // apply rotation to matrix (this = J^T * this * J)
      matrix[p][q] = matrix[q][p] = 0;
      matrix[p][p] -= t * mpq;
      matrix[q][q] += t * mpq;
      float mrp = matrix[r][p];
      float mrq = matrix[r][q];
      matrix[r][p] = matrix[p][r] = cos * mrp - sin * mrq;
      matrix[r][q] = matrix[q][r] = cos * mrq + sin * mrp;

      // apply rotation to rot (rot = rot * J)
      for(int i = 0; i < 3; i++) {
        Vector3& row = orientation[i];
        mrp = row[p];
        mrq = row[q];
        row[p] = cos * mrp - sin * mrq;
        row[q] = cos * mrq + sin * mrp;
      }
    }
  }
}