#include "Precompile.h"

namespace Syx {
  Mat4::Mat4(float r00, float r01, float r02, float r03,
    float r10, float r11, float r12, float r13,
    float r20, float r21, float r22, float r23,
    float r30, float r31, float r32, float r33)
    : mData{ r00, r10, r20, r30,
            r01, r11, r21, r31,
            r02, r12, r22, r32,
            r03, r13, r23, r33 } {
  }

  Mat4::Mat4(const Vec3& c0, const Vec3& c1, const Vec3& c2, const Vec3& c3)
    : mData{ c0.x, c0.y, c0.z, c0.w,
            c1.x, c1.y, c1.z, c1.w,
            c2.x, c2.y, c2.z, c2.w,
            c3.x, c3.y, c3.z, c3.w } {
  }

  //Dot product of left's row lr with right's column rc
  static float _dotRC(int lr, int rc, const Mat4& l, const Mat4& r) {
    return l[0][lr] * r[rc][0] +
      l[1][lr] * r[rc][1] +
      l[2][lr] * r[rc][2] +
      l[3][lr] * r[rc][3];
  }

  Mat4 Mat4::operator*(const Mat4& rhs) const {
    return Mat4(_dotRC(0, 0, *this, rhs), _dotRC(0, 1, *this, rhs), _dotRC(0, 2, *this, rhs), _dotRC(0, 3, *this, rhs),
      _dotRC(1, 0, *this, rhs), _dotRC(1, 1, *this, rhs), _dotRC(1, 2, *this, rhs), _dotRC(1, 3, *this, rhs),
      _dotRC(2, 0, *this, rhs), _dotRC(2, 1, *this, rhs), _dotRC(2, 2, *this, rhs), _dotRC(2, 3, *this, rhs),
      _dotRC(3, 0, *this, rhs), _dotRC(3, 1, *this, rhs), _dotRC(3, 2, *this, rhs), _dotRC(3, 3, *this, rhs));
  }

  static float _dotV(int i, const Mat4& l, const Vec3& v) {
    return l[0][i] * v[0] +
      l[1][i] * v[1] +
      l[2][i] * v[2] +
      l[3][i] * v[3];
  }

  Vec3 Mat4::operator*(const Vec3& v) const {
    return Vec3(_dotV(0, *this, v), _dotV(1, *this, v), _dotV(2, *this, v), _dotV(3, *this, v));
  }

  static float _dotV3(int i, const Mat4& l, const Vec3& v) {
    return l[0][i] * v[0] +
      l[1][i] * v[1] +
      l[2][i] * v[2];
  }

  Mat4 Mat4::affineInverse() const {
    Mat3 rot;
    Vec3 scale, pos;
    decompose(scale, rot, pos);
    //Invert each component
    rot.Transpose();
    scale = scale.Reciprocal();
    pos = -pos;
    //Build up transform again backwards: scale*rotate*translate
    Mat4 result(scale.x*rot.mbx.x, scale.x*rot.mby.x, scale.x*rot.mbz.x, 0.0f,
                scale.y*rot.mbx.y, scale.y*rot.mby.y, scale.y*rot.mbz.y, 0.0f,
                scale.z*rot.mbx.z, scale.z*rot.mby.z, scale.z*rot.mbz.z, 0.0f,
                             0.0f,              0.0f,              0.0f, 1.0f);
    result.mColRow[3][0] = _dotV3(0, result, pos);
    result.mColRow[3][1] = _dotV3(1, result, pos);
    result.mColRow[3][2] = _dotV3(2, result, pos);
    return result;
  }

  void Mat4::decompose(Vec3& scale, Mat3& rotate, Vec3& translate) const {
    translate = Vec3(mColRow[3][0], mColRow[3][1], mColRow[3][2]);
    rotate = Mat3(mColRow[0][0], mColRow[1][0], mColRow[2][0],
                  mColRow[0][1], mColRow[1][1], mColRow[2][1],
                  mColRow[0][2], mColRow[1][2], mColRow[2][2]);
    scale = Vec3(rotate.mbx.Length(), rotate.mby.Length(), rotate.mbz.Length());
    rotate.mbx /= scale.x;
    rotate.mby /= scale.y;
    rotate.mbz /= scale.z;
  }

  void Mat4::decompose(Mat3& rotate, Vec3& translate) const {
    translate = Vec3(mColRow[3][0], mColRow[3][1], mColRow[3][2]);
    rotate = Mat3(mColRow[0][0], mColRow[1][0], mColRow[2][0],
                  mColRow[0][1], mColRow[1][1], mColRow[2][1],
                  mColRow[0][2], mColRow[1][2], mColRow[2][2]);
  }

  Mat4 Mat4::zero() {
    Mat4 result;
    std::memset(&result[0][0], 0, sizeof(Mat4));
    return result;
  }

  Mat4 Mat4::identity() {
    return Mat4(1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f);
  }

  Mat4 Mat4::transform(const Vec3& scale, const Mat3& rotate, const Vec3& translate) {
    //translate*rotate*scale
    return Mat4(rotate.mbx.x*scale.x, rotate.mby.x*scale.y, rotate.mbz.x*scale.z, translate.x,
                rotate.mbx.y*scale.x, rotate.mby.y*scale.y, rotate.mbz.y*scale.z, translate.y,
                rotate.mbx.z*scale.x, rotate.mby.z*scale.y, rotate.mbz.z*scale.z, translate.z,
                                0.0f,                 0.0f,                 0.0f,        1.0f);
  }

  Mat4 Mat4::transform(const Vec3& scale, const Quat& rotate, const Vec3& translate) {
    return Mat4::transform(scale, rotate.ToMatrix(), translate);
  }

  Mat4 Mat4::transform(const Mat3& rotate, const Vec3& translate) {
    //translate*rotate*scale
    return Mat4(rotate.mbx.x, rotate.mby.x, rotate.mbz.x, translate.x,
                rotate.mbx.y, rotate.mby.y, rotate.mbz.y, translate.y,
                rotate.mbx.z, rotate.mby.z, rotate.mbz.z, translate.z,
                        0.0f,         0.0f,         0.0f,        1.0f);
  }

  Mat4 Mat4::transform(const Quat& rotate, const Vec3& translate) {
    return Mat4::transform(rotate.ToMatrix(), translate);
  }

  Mat4 Mat4::scale(const Vec3& scale) {
    return Mat4(scale.x,    0.0f,    0.0f, 0.0f,
                   0.0f, scale.y,    0.0f, 0.0f,
                   0.0f,    0.0f, scale.z, 0.0f,
                   0.0f,    0.0f,    0.0f, 1.0f);
  }

  Mat4 Mat4::rotate(const Mat3& rotate) {
    return Mat4(rotate.mbx.x, rotate.mby.x, rotate.mbz.x, 0.0f,
                rotate.mbx.y, rotate.mby.y, rotate.mbz.y, 0.0f,
                rotate.mbx.z, rotate.mby.z, rotate.mbz.z, 0.0f,
                        0.0f,         0.0f,         0.0f, 1.0f);
  }

  Mat4 Mat4::rotate(const Quat& rotate) {
    return Mat4::rotate(rotate.ToMatrix());
  }

  Mat4 Mat4::translate(const Vec3& translate) {
    return Mat4(1.0f, 0.0f, 0.0f, translate.x,
                0.0f, 1.0f, 0.0f, translate.y,
                0.0f, 0.0f, 1.0f, translate.z,
                0.0f, 0.0f, 0.0f,        1.0f);
  }

  Mat4 Mat4::orthographic(float width, float height, float near, float far) {
    float farmnear = far - near;
    return Mat4(1.0f/width,        0.0f,           0.0f,                   0.0f,
                      0.0f, 1.0f/height,           0.0f,                   0.0f,
                      0.0f,        0.0f, -2.0f/farmnear, -(far + near)/farmnear,
                      0.0f,        0.0f,           0.0f,                   1.0f);
  }

  Mat4 Mat4::perspective(float fovX, float fovY, float near, float far) {
    float farmnear = far - near;
    return Mat4(std::atan(fovX*0.5f),                 0.0f,                   0.0f,                    0.0f,
                                0.0f, std::atan(fovY*0.5f),                   0.0f,                    0.0f,
                                0.0f,                 0.0f, -(far + near)/farmnear, -2.0f*far*near/farmnear,
                                0.0f,                 0.0f,                  -1.0f,                    0.0f);
  }

  static float _colLen(const Mat4& m, int i) {
    float x = m[i][0], y = m[i][1], z = m[i][2];
    return std::sqrt(x*x + y*y + z*z);
  }

  Vec3 Mat4::getScale() const {
    return Vec3(_colLen(*this, 0), _colLen(*this, 1), _colLen(*this, 2));
  }

  Vec3 Mat4::getTranslate() const {
    return Vec3(mColRow[3][0], mColRow[3][1], mColRow[3][2]);
  }

  Quat Mat4::getRotQ() const {
    return getRotM().ToQuat();
  }

  Mat3 Mat4::getRotM() const {
    Mat3 result(mColRow[0][0], mColRow[1][0], mColRow[2][0],
                mColRow[0][1], mColRow[1][1], mColRow[2][1],
                mColRow[0][2], mColRow[1][2], mColRow[2][2]);
    result.mbx.Normalize();
    result.mby.Normalize();
    result.mbz.Normalize();
    return result;
  }

  void Mat4::setScale(const Vec3& scale) {
    Vec3 os = getScale();
    float sx = scale.x/os.x, sy = scale.y/os.y, sz = scale.z/os.x;
    mColRow[0][0] *= sx; mColRow[0][1] *= sx; mColRow[0][2] *= sx;
    mColRow[1][0] *= sy; mColRow[1][1] *= sy; mColRow[1][2] *= sy;
    mColRow[2][0] *= sz; mColRow[2][1] *= sz; mColRow[2][2] *= sz;
  }

  void Mat4::setTranslate(const Vec3& translate) {
    mColRow[3][0] = translate.x;
    mColRow[3][1] = translate.y;
    mColRow[3][2] = translate.z;
  }

  void Mat4::setRot(const Mat3& rot) {
    Vec3 s = getScale();
    mColRow[0][0] = rot[0][0]*s.x; mColRow[0][1] = rot[0][1]*s.x, mColRow[0][2] = rot[0][2]*s.x;
    mColRow[1][0] = rot[1][0]*s.y; mColRow[1][1] = rot[1][1]*s.y, mColRow[1][2] = rot[1][2]*s.y;
    mColRow[2][0] = rot[2][0]*s.z; mColRow[2][1] = rot[2][1]*s.z, mColRow[2][2] = rot[2][2]*s.z;
  }

  void Mat4::setRot(const Quat& rot) {
    setRot(rot.ToMatrix());
  }
}