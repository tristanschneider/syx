#pragma once

namespace Syx {
  struct Matrix2 {
    //Doesn't initialize anything
    Matrix2(void) {}
    //Column 0 and 1
    Matrix2(const Vector2& bx, const Vector2& by): mbx(bx), mby(by) {}
    //Column 0(bxx, bxy) and column 1(byx byy)
    Matrix2(float bxx, float byx, float bxy, float byy): mbx(bxx, bxy), mby(byx, byy) {}

    Matrix2 operator+(const Matrix2& rhs) const;
    Matrix2 operator-(const Matrix2& rhs) const;
    Matrix2 operator*(const Matrix2& rhs) const;
    Vector2 operator*(const Vector2& rhs) const;

    bool operator==(const Matrix2& rhs) const;
    bool operator!=(const Matrix2& rhs) const;

    Matrix2& operator+=(const Matrix2& rhs);
    Matrix2& operator-=(const Matrix2& rhs);
    Matrix2& operator*=(const Matrix2& rhs);

    Matrix2 Transposed(void) const;
    Matrix2 TransposedMultiply(const Matrix2& rhs) const;
    Vector2 TransposedMultiply(const Vector2& rhs) const;

    static Matrix2 Rotate(float cosAngle, float sinAngle);
    static Matrix2 Rotate(float ccwRadians);
    //Inputs assumed to be normalized
    static Matrix2 Rotate(const Vector2& from, const Vector2& to);
    //Construct rotation matrix from normalized up vector
    static Matrix2 RotationFromUp(const Vector2& up);
    //Construct rotation matrix from normalized right vector
    static Matrix2 RotationFromRight(const Vector2& right);
    static Matrix2 Scale(const Vector2& scale);
    static Matrix2 Scale(float scaleX, float scaleY);

    static const Matrix2 sZero;
    static const Matrix2 sIdentity;

    //Colum vectors representing the basis vectors x and y of the rotation matrix
    Vector2 mbx;
    Vector2 mby;
  };

  Matrix2 operator*(float scalar, const Matrix2& mat);
}