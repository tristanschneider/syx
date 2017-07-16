#pragma once
#include "SyxSVec3.h"

#ifdef SENABLED
namespace Syx {
  struct SQuat;
  struct Mat3;

  struct SMat3 {
    SMat3(void) {}
    //First letter is row, second is column
    SMat3(float aa, float ab, float ac, float ba, float bb, float bc, float ca, float cb, float cc);
    SMat3(SFloats bx, SFloats by, SFloats bz);
    SMat3(SFloats diag);

    void Store(Mat3& store) const;

    SMat3 operator*(const SMat3& rhs) const;
    SMat3& operator*=(const SMat3& rhs);
    SFloats operator*(SFloats rhs) const;
    SMat3 operator+(const SMat3& rhs) const;
    SMat3& operator+=(const SMat3& rhs);
    SMat3 operator-(const SMat3& rhs) const;
    SMat3& operator-=(const SMat3& rhs);

    SFloats ToQuat(void) const;

    SMat3 Transposed(void) const;
    void Transpose(void);
    //Not faster when using SIMD versions, so if possible, cache the transpose and re-use it
    SMat3 TransposedMultiply(const SMat3& rhs) const;
    SFloats TransposedMultiply(SFloats rhs) const;

    SMat3 Inverse(void) const;
    SMat3 Inverse(SFloats det) const;
    SFloats Determinant(void) const;

    static const SMat3 Identity;
    static const SMat3 Zero;

    //Basis vectors, first, second and third columns respectively
    SFloats mbx;
    SFloats mby;
    SFloats mbz;
  };

  SMat3 operator*(SFloats lhs, const SMat3& rhs);
  SMat3 ToSMat3(const Mat3& mat);
  Mat3 ToMat3(const SMat3& mat);
}
#endif