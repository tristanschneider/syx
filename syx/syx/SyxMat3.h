#pragma once

namespace Syx {
  struct Vec3;
  struct Quat;

  struct Mat3 {
    Mat3(void) {}
    //First letter is row, second is column
    Mat3(float aa, float ab, float ac, float ba, float bb, float bc, float ca, float cb, float cc);
    //Constructor for symmetric matrix
    Mat3(float a, float b, float c, float d, float e, float f);
    Mat3(const Vec3& bx, const Vec3& by, const Vec3& bz);
    Mat3(const Vec3& diag);

    //Result of multiplication of this matrix with a scale matrix where scale indicates the diagonal
    Mat3 Scaled(const Vec3& scale) const;
    void SetDiagonal(float x, float y, float z);
    void SetDiagonal(const Vec3& diag);
    Vec3 GetDiagonal(void) const;
    void SetRow(int index, const Vec3& row);
    void SetRow(int index, float x, float y, float z);
    float& Get(int row, int col);
    const float& Get(int row, int col) const;
    Vec3 GetRow(int index) const;
    const Vec3& GetCol(int index) const;
    Vec3& GetCol(int index);
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

    Quat ToQuat();

    Mat3 Transposed() const;
    void Transpose();
    Mat3 TransposedMultiply(const Mat3& rhs) const;
    Vec3 TransposedMultiply(const Vec3& rhs) const;

    Mat3 Inverse() const;
    Mat3 Inverse(float det) const;
    float Determinant() const;

    void Diagonalize();

    static Mat3 AxisAngle(const Vec3& axis, float cosAngle, float sinAngle);
    static Mat3 AxisAngle(const Vec3& axis, float angle);
    static Mat3 XRot(float angle);
    static Mat3 YRot(float angle);
    static Mat3 ZRot(float angle);
    static Mat3 XRot(float sinAngle, float cosAngle);
    static Mat3 YRot(float sinAngle, float cosAngle);
    static Mat3 ZRot(float sinAngle, float cosAngle);

    static Mat3 OuterProduct(const Vec3& lhs, const Vec3& rhs);
    //Outer product of vector with itself
    static Mat3 OuterProduct(const Vec3& v);

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