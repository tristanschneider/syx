#pragma once

namespace Syx {
  struct Vec3;
  struct Quat;
  struct Mat3;

  struct Mat4 {
    Mat4() {}
    Mat4(float r00, float r01, float r02, float r03,
         float r10, float r11, float r12, float r13,
         float r20, float r21, float r22, float r23,
         float r30, float r31, float r32, float r33);

    Mat4(const Vec3& c0, const Vec3& c1, const Vec3& c2, const Vec3& c3);

    Mat4 operator*(const Mat4& rhs) const;
    Vec3 operator*(const Vec3& v) const;
    bool operator==(const Mat4& rhs) const;

    float* operator[](int i) {
      return mColRow[i];
    }
    const float* operator[](int i) const {
      return mColRow[i];
    }

    Mat4 affineInverse() const;

    void decompose(Vec3& scale, Mat3& rotate, Vec3& translate) const;
    //Decompose transform that has unit scale
    void decompose(Mat3& rotate, Vec3& translate) const;
    Vec3 getScale() const;
    Vec3 getTranslate() const;
    Quat getRotQ() const;
    Mat3 getRotM() const;
    void setScale(const Vec3& scale);
    void setTranslate(const Vec3& translate);
    void setRot(const Mat3& rot);
    void setRot(const Quat& rot);

    static Mat4 zero();
    static Mat4 identity();
    static Mat4 transform(const Vec3& scale, const Mat3& rotate, const Vec3& translate);
    static Mat4 transform(const Vec3& scale, const Quat& rotate, const Vec3& translate);
    //Unit scale
    static Mat4 transform(const Mat3& rotate, const Vec3& translate);
    static Mat4 transform(const Quat& rotate, const Vec3& translate);
    static Mat4 scale(const Vec3& scale);
    static Mat4 rotate(const Mat3& rotate);
    static Mat4 rotate(const Quat& rotate);
    static Mat4 translate(const Vec3& translate);
    static Mat4 perspective(float fovX, float fovY, float near, float far);
    static Mat4 orthographic(float width, float height, float near, float far);

    //Layout to match opengl
    union {
      float mData[16];
      // mColRow[1][2] means first column vector, second component so basisY.z
      float mColRow[4][4];
    };
  };
}