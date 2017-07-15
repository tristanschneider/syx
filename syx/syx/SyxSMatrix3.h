#pragma once
#include "SyxSVector3.h"

#ifdef SENABLED
namespace Syx {
  struct SQuat;
  struct Matrix3;

  struct SMatrix3 {
    SMatrix3(void) {}
    //First letter is row, second is column
    SMatrix3(float aa, float ab, float ac, float ba, float bb, float bc, float ca, float cb, float cc);
    SMatrix3(SFloats bx, SFloats by, SFloats bz);
    SMatrix3(SFloats diag);

    void Store(Matrix3& store) const;

    SMatrix3 operator*(const SMatrix3& rhs) const;
    SMatrix3& operator*=(const SMatrix3& rhs);
    SFloats operator*(SFloats rhs) const;
    SMatrix3 operator+(const SMatrix3& rhs) const;
    SMatrix3& operator+=(const SMatrix3& rhs);
    SMatrix3 operator-(const SMatrix3& rhs) const;
    SMatrix3& operator-=(const SMatrix3& rhs);

    SFloats ToQuat(void) const;

    SMatrix3 Transposed(void) const;
    void Transpose(void);
    //Not faster when using SIMD versions, so if possible, cache the transpose and re-use it
    SMatrix3 TransposedMultiply(const SMatrix3& rhs) const;
    SFloats TransposedMultiply(SFloats rhs) const;

    SMatrix3 Inverse(void) const;
    SMatrix3 Inverse(SFloats det) const;
    SFloats Determinant(void) const;

    static const SMatrix3 Identity;
    static const SMatrix3 Zero;

    //Basis vectors, first, second and third columns respectively
    SFloats mbx;
    SFloats mby;
    SFloats mbz;
  };

  SMatrix3 operator*(SFloats lhs, const SMatrix3& rhs);
  SMatrix3 ToSMatrix3(const Matrix3& mat);
  Matrix3 ToMatrix3(const SMatrix3& mat);
}
#endif