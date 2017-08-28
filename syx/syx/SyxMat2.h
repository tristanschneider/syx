#pragma once

namespace Syx {
  struct Matrix2 {
    //Doesn't initialize anything
    Matrix2() {}
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

    Matrix2 transposed(void) const;
    Matrix2 transposedMultiply(const Matrix2& rhs) const;
    Vector2 transposedMultiply(const Vector2& rhs) const;

    static Matrix2 rotate(float cosAngle, float sinAngle);
    static Matrix2 rotate(float ccwRadians);
    //Inputs assumed to be normalized
    static Matrix2 rotate(const Vector2& from, const Vector2& to);
    //Construct rotation matrix from normalized up vector
    static Matrix2 rotationFromUp(const Vector2& up);
    //Construct rotation matrix from normalized right vector
    static Matrix2 rotationFromRight(const Vector2& right);
    static Matrix2 scale(const Vector2& s);
    static Matrix2 scale(float scaleX, float scaleY);

    static const Matrix2 sZero;
    static const Matrix2 sIdentity;

    //Colum vectors representing the basis vectors x and y of the rotation matrix
    Vector2 mbx;
    Vector2 mby;
  };

  Matrix2 operator*(float scalar, const Matrix2& mat);
}