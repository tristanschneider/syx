#include "Precompile.h"
#include "SyxSMatrix3.h"
#include "SyxSVector3.h"
#include "SyxSIMD.h"
#include "SyxSQuaternion.h"
#include "SyxMatrix3.h"
#include "SyxQuaternion.h"

#ifdef SENABLED
namespace Syx {
  const SMatrix3 SMatrix3::Identity = SMatrix3(1.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 1.0f);
  const SMatrix3 SMatrix3::Zero = SMatrix3(0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f);

  SMatrix3::SMatrix3(float aa, float ab, float ac,
    float ba, float bb, float bc,
    float ca, float cb, float cc):
    mbx(SLoadFloats(aa, ba, ca)), mby(SLoadFloats(ab, bb, cb)), mbz(SLoadFloats(ac, bc, cc)) {
  }

  SMatrix3::SMatrix3(SFloats bx, SFloats by, SFloats bz) : mbx(bx), mby(by), mbz(bz) {}

  SMatrix3::SMatrix3(SFloats diag) {
    mbx = SAnd(diag, SVector3::BitsX);
    mby = SAnd(diag, SVector3::BitsY);
    mbz = SAnd(diag, SVector3::BitsZ);
  }

  //Dot xyz and leave the result in [0]
  SFloats Dot3(SFloats lhs, SFloats rhs) {
    SFloats mul = SMulAll(lhs, rhs);
    //yxy
    SFloats temp = SShuffle(mul, 1, 0, 1, 3);
    SFloats result = SAddAll(mul, temp);
    //zzx
    temp = SShuffle(mul, 2, 2, 0, 3);
    return SAddAll(result, temp);
  }

  //Produce one column of the multiplied matrix, given the column (vec) of the matrix to multiply with
  /*
  [a b c] [j] [aj+bk+cl]
  [d e f]*[k]=[dj+ek+fl]
  [g h i] [l] [gj+hk+il]*/
  SFloats MultColumn(const SMatrix3& mat, SFloats vec) {
    SFloats result = SMulAll(mat.mbx, SShuffle(vec, 0, 0, 0, 0));
    result = SAddAll(result, SMulAll(mat.mby, SShuffle(vec, 1, 1, 1, 1)));
    result = SAddAll(result, SMulAll(mat.mbz, SShuffle(vec, 2, 2, 2, 2)));
    return result;
  }

  /*
  [a b c] [j k l] [aj+bm+cp, ak+bn+cq, al+bo+cr]
  [d e f]*[m n o]=[dj+em+fp, dk+en+fq, dl+eo+fr]
  [g h i] [p q r] [gj+hm+ip, gk+hn+iq, gl+ho+ir]*/
  SMatrix3 SMatrix3::operator*(const SMatrix3& rhs) const {
    SMatrix3 result;
    result.mbx = MultColumn(*this, rhs.mbx);
    result.mby = MultColumn(*this, rhs.mby);
    result.mbz = MultColumn(*this, rhs.mbz);
    return result;
  }

  SMatrix3& SMatrix3::operator*=(const SMatrix3& rhs) {
    return *this = *this * rhs;
  }

  SFloats SMatrix3::operator*(SFloats rhs) const {
    return MultColumn(*this, rhs);
  }

  SMatrix3 SMatrix3::operator+(const SMatrix3& rhs) const {
    return SMatrix3(SAddAll(mbx, rhs.mbx), SAddAll(mby, rhs.mby), SAddAll(mbz, rhs.mbz));
  }

  SMatrix3& SMatrix3::operator+=(const SMatrix3& rhs) {
    mbx = SAddAll(mbx, rhs.mbx);
    mby = SAddAll(mby, rhs.mby);
    mbz = SAddAll(mbz, rhs.mbz);
    return *this;
  }

  SMatrix3 SMatrix3::operator-(const SMatrix3& rhs) const {
    return SMatrix3(SSubAll(mbx, rhs.mbx), SSubAll(mby, rhs.mby), SSubAll(mbz, rhs.mbz));
  }

  SMatrix3& SMatrix3::operator-=(const SMatrix3& rhs) {
    mbx = SSubAll(mbx, rhs.mbx);
    mby = SSubAll(mby, rhs.mby);
    mbz = SSubAll(mbz, rhs.mbz);
    return *this;
  }

  SMatrix3 SMatrix3::Transposed(void) const {
    SMatrix3 result;
    //[r0,?,r1,?]
    result.mbx = SShuffle2(mbx, mby, 0, 3, 0, 3);
    //[r0,r1,?,?]
    result.mbx = SShuffle(result.mbx, 0, 2, 3, 3);
    //[r0,r1,r2,?]
    result.mbx = SShuffle2(result.mbx, mbz, 0, 1, 0, 3);

    //Similar as above for other rows
    result.mby = SShuffle2(mbx, mby, 1, 3, 1, 3);
    result.mby = SShuffle(result.mby, 0, 2, 3, 3);
    result.mby = SShuffle2(result.mby, mbz, 0, 1, 1, 3);

    result.mbz = SShuffle2(mbx, mby, 2, 3, 2, 3);
    result.mbz = SShuffle(result.mbz, 0, 2, 3, 3);
    result.mbz = SShuffle2(result.mbz, mbz, 0, 1, 2, 3);
    return result;
  }

  void SMatrix3::Transpose(void) {
    *this = Transposed();
  }

  SMatrix3 SMatrix3::TransposedMultiply(const SMatrix3& rhs) const {
    return Transposed() * rhs;
  }

  SFloats SMatrix3::TransposedMultiply(SFloats rhs) const {
    return Transposed() * rhs;
  }

  /*Assume this matrix for comments below:
  [a,b,c]
  [d,e,f]
  [g,h,i]*/
  SMatrix3 SMatrix3::Inverse(void) const {
    SMatrix3 result;
    SFloats lhs, rhs, det;

    //[e,h,b]*[i,c,f]
    lhs = SMulAll(SShuffle(mby, 1, 2, 0, 3), SShuffle(mbz, 2, 0, 1, 3));
    //[h,b,e]*[f,i,c]
    rhs = SMulAll(SShuffle(mby, 2, 0, 1, 3), SShuffle(mbz, 1, 2, 0, 3));
    //Subtract each multiplication pair from the corresponding other one
    result.mbx = SSubAll(lhs, rhs);
    det = SMulAll(mbx, result.mbx);

    //[g,a,d]*[f,i,c]
    lhs = SMulAll(SShuffle(mbx, 2, 0, 1, 3), SShuffle(mbz, 1, 2, 0, 3));
    //[d,g,a]*[i,c,f]
    rhs = SMulAll(SShuffle(mbx, 1, 2, 0, 3), SShuffle(mbz, 2, 0, 1, 3));
    result.mby = SSubAll(lhs, rhs);

    //[d,g,a]*[h,b,e]
    lhs = SMulAll(SShuffle(mbx, 1, 2, 0, 3), SShuffle(mby, 2, 0, 1, 3));
    //[g,a,d]*[e,h,b]
    rhs = SMulAll(SShuffle(mbx, 2, 0, 1, 3), SShuffle(mby, 1, 2, 0, 3));
    result.mbz = SSubAll(lhs, rhs);

    //Easier to do the shuffles here than trying to shuffle with the above math. I imagine it doesn't cost much either
    result.Transpose();

    det = SAddLower(det, SShuffle(det, 1, 1, 2, 3));
    det = SAddLower(det, SShuffle(det, 2, 1, 2, 3));
    det = SShuffle(det, 0, 0, 0, 0);
    det = SVector3::SafeDivide(SVector3::Identity, det);

    return det * result;
  }

  SMatrix3 SMatrix3::Inverse(SFloats det) const {
    SFloats invDet = SVector3::SafeDivide(SVector3::Identity, det);
    SMatrix3 result;
    SFloats lhs, rhs;

    //[e,h,b]*[i,c,f]
    lhs = SMulAll(SShuffle(mby, 1, 2, 0, 3), SShuffle(mbz, 2, 0, 1, 3));
    //[h,b,e]*[f,i,c]
    rhs = SMulAll(SShuffle(mby, 2, 0, 1, 3), SShuffle(mbz, 1, 2, 0, 3));
    //Subtract each multiplication pair from the corresponding other one
    result.mbx = SSubAll(lhs, rhs);

    //[g,a,d]*[f,i,c]
    lhs = SMulAll(SShuffle(mbx, 2, 0, 1, 3), SShuffle(mbz, 1, 2, 0, 3));
    //[d,g,a]*[i,c,f]
    rhs = SMulAll(SShuffle(mbx, 1, 2, 0, 3), SShuffle(mbz, 2, 0, 1, 3));
    result.mby = SSubAll(lhs, rhs);

    //[d,g,a]*[h,b,e]
    lhs = SMulAll(SShuffle(mbx, 1, 2, 0, 3), SShuffle(mby, 2, 0, 1, 3));
    //[g,a,d]*[e,h,b]
    rhs = SMulAll(SShuffle(mbx, 2, 0, 1, 3), SShuffle(mby, 1, 2, 0, 3));
    result.mbz = SSubAll(lhs, rhs);

    //Easier to do the shuffles here than trying to shuffle with the above math. I imagine it doesn't cost much either
    result.Transpose();
    return invDet * result;
  }

  SFloats SMatrix3::Determinant(void) const {
    //[e,h,b]*[i,c,f]
    SFloats lhs = SMulAll(SShuffle(mby, 1, 2, 0, 3), SShuffle(mbz, 2, 0, 1, 3));
    //[h,b,e]*[f,i,c]
    SFloats rhs = SMulAll(SShuffle(mby, 2, 0, 1, 3), SShuffle(mbz, 1, 2, 0, 3));
    //Subtract each multiplication pair from the corresponding other one, and scale by the first column element of the row
    SFloats result = SMulAll(mbx, SSubAll(lhs, rhs));

    //Add the x,y,z terms and splat the result
    result = SAddLower(result, SShuffle(result, 1, 1, 2, 3));
    result = SAddLower(result, SShuffle(result, 2, 1, 2, 3));
    return SShuffle(result, 0, 0, 0, 0);
  }

  SFloats SMatrix3::ToQuat(void) const {
    //This has so many branches and single scalar operations I don't think it's worth it to try and SIMD it.
    SAlign Matrix3 mat;
    SVector3::Store(mbx, mat.mbx);
    SVector3::Store(mby, mat.mby);
    SVector3::Store(mbz, mat.mbz);
    SAlign Quat quat = mat.ToQuat();
    return SLoadAll(&quat.mV.x);
  }

  SMatrix3 operator*(SFloats lhs, const SMatrix3& rhs) {
    return SMatrix3(SMulAll(lhs, rhs.mbx), SMulAll(lhs, rhs.mby), SMulAll(lhs, rhs.mbz));
  }

  SMatrix3 ToSMatrix3(const Matrix3& mat) {
    return SMatrix3(ToSVector3(mat.mbx), ToSVector3(mat.mby), ToSVector3(mat.mbz));
  }

  Matrix3 ToMatrix3(const SMatrix3& mat) {
    return Matrix3(ToVector3(mat.mbx), ToVector3(mat.mby), ToVector3(mat.mbz));
  }

  void SMatrix3::Store(Matrix3& store) const {
    SVector3::Store(mbx, store.mbx);
    SVector3::Store(mby, store.mby);
    SVector3::Store(mbz, store.mbz);
  }
}
#endif