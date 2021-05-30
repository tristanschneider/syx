#pragma once
#include "SyxVec3.h"

namespace Syx {
  struct Quat;

  struct Mat3 {
    Mat3() {}
    //First letter is row, second is column
    Mat3(float aa, float ab, float ac, float ba, float bb, float bc, float ca, float cb, float cc);
    //Constructor for symmetric matrix
    Mat3(float a, float b, float c, float d, float e, float f);
    Mat3(const Vec3& bx, const Vec3& by, const Vec3& bz);
    Mat3(const Vec3& diag);

    //Result of multiplication of this matrix with a scale matrix where scale indicates the diagonal
    Mat3 scaled(const Vec3& scale) const;
    void setDiagonal(float x, float y, float z);
    void setDiagonal(const Vec3& diag);
    Vec3 getDiagonal(void) const;
    void setRow(int index, const Vec3& row);
    void setRow(int index, float x, float y, float z);
    float& get(int row, int col);
    const float& get(int row, int col) const;
    Vec3 getRow(int index) const;
    const Vec3& getCol(int index) const;
    Vec3& getCol(int index);
    //Get column. Careful, as this results in [col][row]
    Vec3& operator[](int index);
    const Vec3& operator[](int index) const;

    Mat3 operator*(const Mat3& rhs) const;
    Mat3& operator*=(const Mat3& rhs);
    Vec3 operator*(const Vec3& rhs) const;
    Mat3 operator*(float rhs) const;
    Mat3& operator*=(float rhs);
    Mat3 operator+(const Mat3& rhs) const;
    Mat3& operator+=(const Mat3& rhs);
    Mat3 operator-(const Mat3& rhs) const;
    Mat3& operator-=(const Mat3& rhs);
    Mat3 operator/(float rhs) const;
    Mat3& operator/=(float rhs);

    Quat toQuat();

    Mat3 transposed() const;
    void transpose();
    Mat3 transposedMultiply(const Mat3& rhs) const;
    Vec3 transposedMultiply(const Vec3& rhs) const;

    Mat3 inverse() const;
    Mat3 inverse(float det) const;
    float determinant() const;

    void diagonalize();

    static Mat3 axisAngle(const Vec3& axis, float cosAngle, float sinAngle);
    static Mat3 axisAngle(const Vec3& axis, float angle);
    static Mat3 xRot(float angle);
    static Mat3 yRot(float angle);
    static Mat3 zRot(float angle);
    static Mat3 xRot(float sinAngle, float cosAngle);
    static Mat3 yRot(float sinAngle, float cosAngle);
    static Mat3 zRot(float sinAngle, float cosAngle);

    static Mat3 outerProduct(const Vec3& lhs, const Vec3& rhs);
    //Outer product of vector with itself
    static Mat3 outerProduct(const Vec3& v);

    static const Mat3 Identity;
    static const Mat3 Zero;

    //Basis vectors, first, second and third columns respectively
    Vec3 mbx;
    Vec3 mby;
    Vec3 mbz;
  };

  Mat3 operator*(float lhs, const Mat3& rhs);
  typedef Mat3 Mat3;
}