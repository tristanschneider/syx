#pragma once

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

    void store(Mat3& store) const;

    SMat3 operator*(const SMat3& rhs) const;
    SMat3& operator*=(const SMat3& rhs);
    SFloats operator*(SFloats rhs) const;
    SMat3 operator+(const SMat3& rhs) const;
    SMat3& operator+=(const SMat3& rhs);
    SMat3 operator-(const SMat3& rhs) const;
    SMat3& operator-=(const SMat3& rhs);

    SFloats toQuat(void) const;

    SMat3 transposed(void) const;
    void transpose(void);
    //Not faster when using SIMD versions, so if possible, cache the transpose and re-use it
    SMat3 transposedMultiply(const SMat3& rhs) const;
    SFloats transposedMultiply(SFloats rhs) const;

    SMat3 inverse(void) const;
    SMat3 inverse(SFloats det) const;
    SFloats determinant(void) const;

    static const SMat3 Identity;
    static const SMat3 Zero;

    //Basis vectors, first, second and third columns respectively
    SFloats mbx;
    SFloats mby;
    SFloats mbz;
  };

  SMat3 operator*(SFloats lhs, const SMat3& rhs);
  SMat3 toSMat3(const Mat3& mat);
  Mat3 toMat3(const SMat3& mat);
}
#endif