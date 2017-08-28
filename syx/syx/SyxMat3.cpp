#include "Precompile.h"

namespace Syx {
  const Mat3 Mat3::Identity = Mat3(1.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 1.0f);
  const Mat3 Mat3::Zero = Mat3(0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f);

  Mat3::Mat3(float aa, float ab, float ac,
    float ba, float bb, float bc,
    float ca, float cb, float cc):
    mbx(aa, ba, ca),
    mby(ab, bb, cb),
    mbz(ac, bc, cc) {
  }

  Mat3::Mat3(const Vec3& bx, const Vec3& by, const Vec3& bz):
    mbx(bx), mby(by), mbz(bz) {
  }

  Mat3::Mat3(float a, float b, float c,
                            float d, float e,
                                     float f)
    : mbx(a, b, c)
    , mby(b, d, e)
    , mbz(c, e, f) {}

  Mat3::Mat3(const Vec3& diag):
    mbx(diag.x, 0.0f, 0.0f),
    mby(0.0f, diag.y, 0.0f),
    mbz(0.0f, 0.0f, diag.z) {
  }

  Mat3 Mat3::scaled(const Vec3& scale) const {
    //Wastes a bunch of non-diagonal multiplications. Can optimize later if I feel like it
    Mat3 temp = Mat3::Identity;
    temp.setDiagonal(scale);
    return *this * temp;
  }

  void Mat3::setDiagonal(float x, float y, float z) {
    mbx.x = x;
    mby.y = y;
    mbz.z = z;
  }

  void Mat3::setDiagonal(const Vec3& diag) {
    setDiagonal(diag.x, diag.y, diag.z);
  }

  Vec3 Mat3::getDiagonal(void) const {
    return Vec3(get(0, 0), get(1, 1), get(2, 2));
  }

  void Mat3::setRow(int index, const Vec3& row) {
    setRow(index, row.x, row.y, row.z);
  }

  void Mat3::setRow(int index, float x, float y, float z) {
    get(index, 0) = x;
    get(index, 1) = y;
    get(index, 2) = z;
  }

  float& Mat3::get(int row, int col) {
    return *(&mbx.x + row + 4*col);
  }

  const float& Mat3::get(int row, int col) const {
    return *(&mbx.x + row + 4*col);
  }

  Vec3 Mat3::getRow(int index) const {
    return Vec3(get(index, 0), get(index, 1), get(index, 2));
  }

  const Vec3& Mat3::getCol(int index) const {
    switch(index) {
      case 0: return mbx;
      case 1: return mby;
      case 2: return mbz;
      default: SyxAssertError(false, "invalid index"); return mbx;
    }
  }

  Vec3& Mat3::getCol(int index) {
    switch(index) {
      case 0: return mbx;
      case 1: return mby;
      case 2: return mbz;
      default: SyxAssertError(false, "invalid index"); return mbx;
    }
  }

  Vec3& Mat3::operator[](int index) {
    return *(&mbx + index);
  }

  const Vec3& Mat3::operator[](int index) const {
    return *(&mbx + index);
  }

  Mat3 Mat3::operator*(const Mat3& rhs) const {
    return Mat3(get(0, 0)*rhs.get(0, 0) + get(0, 1)*rhs.get(1, 0) + get(0, 2)*rhs.get(2, 0),
      get(0, 0)*rhs.get(0, 1) + get(0, 1)*rhs.get(1, 1) + get(0, 2)*rhs.get(2, 1),
      get(0, 0)*rhs.get(0, 2) + get(0, 1)*rhs.get(1, 2) + get(0, 2)*rhs.get(2, 2),

      get(1, 0)*rhs.get(0, 0) + get(1, 1)*rhs.get(1, 0) + get(1, 2)*rhs.get(2, 0),
      get(1, 0)*rhs.get(0, 1) + get(1, 1)*rhs.get(1, 1) + get(1, 2)*rhs.get(2, 1),
      get(1, 0)*rhs.get(0, 2) + get(1, 1)*rhs.get(1, 2) + get(1, 2)*rhs.get(2, 2),

      get(2, 0)*rhs.get(0, 0) + get(2, 1)*rhs.get(1, 0) + get(2, 2)*rhs.get(2, 0),
      get(2, 0)*rhs.get(0, 1) + get(2, 1)*rhs.get(1, 1) + get(2, 2)*rhs.get(2, 1),
      get(2, 0)*rhs.get(0, 2) + get(2, 1)*rhs.get(1, 2) + get(2, 2)*rhs.get(2, 2));
  }

  Mat3& Mat3::operator*=(const Mat3& rhs) {
    return *this = *this * rhs;
  }

  Vec3 Mat3::operator*(const Vec3& rhs) const {
    return Vec3(get(0, 0)*rhs.x + get(0, 1)*rhs.y + get(0, 2)*rhs.z,
      get(1, 0)*rhs.x + get(1, 1)*rhs.y + get(1, 2)*rhs.z,
      get(2, 0)*rhs.x + get(2, 1)*rhs.y + get(2, 2)*rhs.z);
  }

  Mat3 Mat3::operator*(float rhs) const {
    return Mat3(mbx*rhs, mby*rhs, mbz*rhs);
  }

  Mat3& Mat3::operator*=(float rhs) {
    mbx *= rhs;
    mby *= rhs;
    mbz *= rhs;
    return *this;
  }

  Mat3 Mat3::operator+(const Mat3& rhs) const {
    return Mat3(mbx + rhs.mbx, mby + rhs.mby, mbz + rhs.mbz);
  }

  Mat3& Mat3::operator+=(const Mat3& rhs) {
    mbx += rhs.mbx;
    mby += rhs.mby;
    mbz += rhs.mbz;
    return *this;
  }

  Mat3 Mat3::operator-(const Mat3& rhs) const {
    return Mat3(mbx - rhs.mbx, mby - rhs.mby, mbz - rhs.mbz);
  }

  Mat3& Mat3::operator-=(const Mat3& rhs) {
    mbx -= rhs.mbx;
    mby -= rhs.mby;
    mbz -= rhs.mbz;
    return *this;
  }

  Mat3 Mat3::operator/(float rhs) const {
    return *this * (1.0f/rhs);
  }

  Mat3& Mat3::operator/=(float rhs) {
    return *this *= (1.0f/rhs);
  }

  Quat Mat3::toQuat(void) {
    float trace = get(0, 0) + get(1, 1) + get(2, 2);
    if(trace > SYX_EPSILON) {
      float s = sqrt(trace + 1.0f)*2.0f;
      return Quat((get(2, 1) - get(1, 2))/s, (get(0, 2) - get(2, 0))/s, (get(1, 0) - get(0, 1))/s, 0.25f*s);
    }
    if(get(0, 0) > get(1, 1) && get(0, 0) > get(2, 2)) {
      float s = sqrt(1.0f + get(0, 0) - get(1, 1) - get(2, 2))*2.0f;
      return Quat(0.25f*s, (get(0, 1) + get(1, 0))/s, (get(0, 2) + get(2, 0))/s, (get(2, 1) - get(1, 2))/s);
    }
    if(get(1, 1) > get(2, 2)) {
      float s = sqrt(1.0f + get(1, 1) - get(0, 0) - get(2, 2))*2.0f;
      return Quat((get(0, 1) + get(1, 0))/s, 0.25f*s, (get(1, 2) + get(2, 1))/s, (get(0, 2) - get(2, 0))/s);
    }
    float s = sqrt(1.0f + get(2, 2) - get(0, 0) - get(1, 1))*2.0f;
    return Quat((get(0, 2) + get(2, 0))/s, (get(1, 2) + get(2, 1))/s, 0.25f*s, (get(1, 0) - get(0, 1))/s);
  }

  Mat3 Mat3::transposed(void) const {
    return Mat3(get(0, 0), get(1, 0), get(2, 0),
      get(0, 1), get(1, 1), get(2, 1),
      get(0, 2), get(1, 2), get(2, 2));
  }

  void Mat3::transpose(void) {
    std::swap(get(0, 1), get(1, 0));
    std::swap(get(0, 2), get(2, 0));
    std::swap(get(2, 1), get(1, 2));
  }

  Mat3 Mat3::transposedMultiply(const Mat3& rhs) const {
    return Mat3(get(0, 0)*rhs.get(0, 0) + get(1, 0)*rhs.get(1, 0) + get(2, 0)*rhs.get(2, 0),
      get(0, 0)*rhs.get(0, 1) + get(1, 0)*rhs.get(1, 1) + get(2, 0)*rhs.get(2, 1),
      get(0, 0)*rhs.get(0, 2) + get(1, 0)*rhs.get(1, 2) + get(2, 0)*rhs.get(2, 2),

      get(0, 1)*rhs.get(0, 0) + get(1, 1)*rhs.get(1, 0) + get(2, 1)*rhs.get(2, 0),
      get(0, 1)*rhs.get(0, 1) + get(1, 1)*rhs.get(1, 1) + get(2, 1)*rhs.get(2, 1),
      get(0, 1)*rhs.get(0, 2) + get(1, 1)*rhs.get(1, 2) + get(2, 1)*rhs.get(2, 2),

      get(0, 2)*rhs.get(0, 0) + get(1, 2)*rhs.get(1, 0) + get(2, 2)*rhs.get(2, 0),
      get(0, 2)*rhs.get(0, 1) + get(1, 2)*rhs.get(1, 1) + get(2, 2)*rhs.get(2, 1),
      get(0, 2)*rhs.get(0, 2) + get(1, 2)*rhs.get(1, 2) + get(2, 2)*rhs.get(2, 2));
  }

  Vec3 Mat3::transposedMultiply(const Vec3& rhs) const {
    return Vec3(get(0, 0)*rhs.x + get(1, 0)*rhs.y + get(2, 0)*rhs.z,
      get(0, 1)*rhs.x + get(1, 1)*rhs.y + get(2, 1)*rhs.z,
      get(0, 2)*rhs.x + get(1, 2)*rhs.y + get(2, 2)*rhs.z);
  }

  Mat3 Mat3::inverse(void) const {
    return inverse(determinant());
  }

  Mat3 Mat3::inverse(float det) const {
    float invdet = safeDivide(1.0f, det, SYX_EPSILON);

    Mat3 result;
    result.get(0, 0) = (get(1, 1) * get(2, 2) - get(2, 1) * get(1, 2))*invdet;
    result.get(0, 1) = -(get(0, 1) * get(2, 2) - get(0, 2) * get(2, 1))*invdet;
    result.get(0, 2) = (get(0, 1) * get(1, 2) - get(0, 2) * get(1, 1))*invdet;
    result.get(1, 0) = -(get(1, 0) * get(2, 2) - get(1, 2) * get(2, 0))*invdet;
    result.get(1, 1) = (get(0, 0) * get(2, 2) - get(0, 2) * get(2, 0))*invdet;
    result.get(1, 2) = -(get(0, 0) * get(1, 2) - get(1, 0) * get(0, 2))*invdet;
    result.get(2, 0) = (get(1, 0) * get(2, 1) - get(2, 0) * get(1, 1))*invdet;
    result.get(2, 1) = -(get(0, 0) * get(2, 1) - get(2, 0) * get(0, 1))*invdet;
    result.get(2, 2) = (get(0, 0) * get(1, 1) - get(1, 0) * get(0, 1))*invdet;
    return result;
  }

  float Mat3::determinant(void) const {
    return get(0, 0) * (get(1, 1) * get(2, 2) - get(2, 1) * get(1, 2)) -
      get(0, 1) * (get(1, 0) * get(2, 2) - get(1, 2) * get(2, 0)) +
      get(0, 2) * (get(1, 0) * get(2, 1) - get(1, 1) * get(2, 0));
  }

  Mat3 operator*(float lhs, const Mat3& rhs) {
    return rhs * lhs;
  }

  Mat3 Mat3::axisAngle(const Vec3& axis, float cosAngle, float sinAngle) {
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

    return Mat3(x*x*C + c, x*y*C - z*s, x*z*C + y*s,
      y*x*C + z*s, y*y*C + c, y*z*C - x*s,
      z*x*C - y*s, z*y*C + x*s, z*z*C + c);
  }

  Mat3 Mat3::axisAngle(const Vec3& axis, float angle) {
    return axisAngle(axis, cosf(angle), sinf(angle));
  }

  Mat3 Mat3::xRot(float angle) {
    return xRot(sin(angle), cos(angle));
  }

  Mat3 Mat3::yRot(float angle) {
    return yRot(sin(angle), cos(angle));
  }

  Mat3 Mat3::zRot(float angle) {
    return zRot(sin(angle), cos(angle));
  }

  Mat3 Mat3::xRot(float sinAngle, float cosAngle) {
    return Mat3(1.0f, 0.0f, 0.0f,
      0.0f, cosAngle, -sinAngle,
      0.0f, sinAngle, cosAngle);
  }

  Mat3 Mat3::yRot(float sinAngle, float cosAngle) {
    return Mat3(cosAngle, 0.0f, sinAngle,
      0.0f, 1.0f, 0.0f,
      -sinAngle, 0.0f, cosAngle);
  }

  Mat3 Mat3::zRot(float sinAngle, float cosAngle) {
    return Mat3(cosAngle, -sinAngle, 0.0f,
      sinAngle, cosAngle, 0.0f,
      0.0f, 0.0f, 1.0f);
  }

  Mat3 Mat3::outerProduct(const Vec3& lhs, const Vec3& rhs) {
    return Mat3(lhs.x*rhs.x, lhs.x*rhs.y, lhs.x*rhs.z,
                   lhs.y*rhs.x, lhs.y*rhs.y, lhs.y*rhs.z,
                   lhs.z*rhs.x, lhs.z*rhs.y, lhs.z*rhs.z);
  }

  Mat3 Mat3::outerProduct(const Vec3& v) {
    float xy = v.x*v.y;
    float xz = v.x*v.z;
    float yz = v.y*v.z;
    return Mat3(v.x*v.x, xy, xz,
                  xy, v.y*v.y, yz,
                  xz, yz, v.z*v.z);
  }

  //From Bullet
  void Mat3::diagonalize(void) {
    Mat3 orientation = Mat3::Zero;
    Mat3& matrix = *this;

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
        Vec3& row = orientation[i];
        mrp = row[p];
        mrq = row[q];
        row[p] = cos * mrp - sin * mrq;
        row[q] = cos * mrq + sin * mrp;
      }
    }
  }
}