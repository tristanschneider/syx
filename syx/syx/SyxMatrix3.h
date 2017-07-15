#pragma once
#include "SyxVector3.h"

namespace Syx {
  struct Vector3;
  struct Quat;

  struct Matrix3 {
    Matrix3(void) {}
    //First letter is row, second is column
    Matrix3(float aa, float ab, float ac, float ba, float bb, float bc, float ca, float cb, float cc);
    //Constructor for symmetric matrix
    Matrix3(float a, float b, float c, float d, float e, float f);
    Matrix3(const Vector3& bx, const Vector3& by, const Vector3& bz);
    Matrix3(const Vector3& diag);

    //Result of multiplication of this matrix with a scale matrix where scale indicates the diagonal
    Matrix3 Scaled(const Vector3& scale) const;
    void SetDiagonal(float x, float y, float z);
    void SetDiagonal(const Vector3& diag);
    Vector3 GetDiagonal(void) const;
    void SetRow(int index, const Vector3& row);
    void SetRow(int index, float x, float y, float z);
    float& Get(int row, int col);
    const float& Get(int row, int col) const;
    Vector3 GetRow(int index) const;
    const Vec3& GetCol(int index) const;
    Vec3& GetCol(int index);
    //Get column. Careful, as this results in [col][row]
    Vector3& operator[](int index);
    const Vector3& operator[](int index) const;

    Matrix3 operator*(const Matrix3& rhs) const;
    Matrix3& operator*=(const Matrix3& rhs);
    Vector3 operator*(const Vector3& rhs) const;
    Matrix3 operator*(float rhs) const;
    Matrix3& operator*=(float rhs);
    Matrix3 operator+(const Matrix3& rhs) const;
    Matrix3& operator+=(const Matrix3& rhs);
    Matrix3 operator-(const Matrix3& rhs) const;
    Matrix3& operator-=(const Matrix3& rhs);
    Matrix3 operator/(float rhs) const;
    Matrix3& operator/=(float rhs);

    Quat ToQuat();

    Matrix3 Transposed() const;
    void Transpose();
    Matrix3 TransposedMultiply(const Matrix3& rhs) const;
    Vector3 TransposedMultiply(const Vector3& rhs) const;

    Matrix3 Inverse() const;
    Matrix3 Inverse(float det) const;
    float Determinant() const;

    void Diagonalize();

    static Matrix3 AxisAngle(const Vector3& axis, float cosAngle, float sinAngle);
    static Matrix3 AxisAngle(const Vector3& axis, float angle);
    static Matrix3 XRot(float angle);
    static Matrix3 YRot(float angle);
    static Matrix3 ZRot(float angle);
    static Matrix3 XRot(float sinAngle, float cosAngle);
    static Matrix3 YRot(float sinAngle, float cosAngle);
    static Matrix3 ZRot(float sinAngle, float cosAngle);

    static Matrix3 OuterProduct(const Vector3& lhs, const Vector3& rhs);
    //Outer product of vector with itself
    static Matrix3 OuterProduct(const Vector3& v);

    static const Matrix3 Identity;
    static const Matrix3 Zero;

    //Basis vectors, first, second and third columns respectively
    Vector3 mbx;
    Vector3 mby;
    Vector3 mbz;
  };

  Matrix3 operator*(float lhs, const Matrix3& rhs);
  typedef Matrix3 Mat3;
}