#include "Precompile.h"

#ifdef SENABLED
namespace Syx {
  const SMat3 SMat3::Identity = SMat3(1.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 1.0f);
  const SMat3 SMat3::Zero = SMat3(0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f);

  SMat3::SMat3(float aa, float ab, float ac,
    float ba, float bb, float bc,
    float ca, float cb, float cc):
    mbx(SLoadFloats(aa, ba, ca)), mby(SLoadFloats(ab, bb, cb)), mbz(SLoadFloats(ac, bc, cc)) {
  }

  SMat3::SMat3(SFloats bx, SFloats by, SFloats bz) : mbx(bx), mby(by), mbz(bz) {}

  SMat3::SMat3(SFloats diag) {
    mbx = SAnd(diag, SVec3::BitsX);
    mby = SAnd(diag, SVec3::BitsY);
    mbz = SAnd(diag, SVec3::BitsZ);
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
  SFloats MultColumn(const SMat3& mat, SFloats vec) {
    SFloats result = SMulAll(mat.mbx, SShuffle(vec, 0, 0, 0, 0));
    result = SAddAll(result, SMulAll(mat.mby, SShuffle(vec, 1, 1, 1, 1)));
    result = SAddAll(result, SMulAll(mat.mbz, SShuffle(vec, 2, 2, 2, 2)));
    return result;
  }

  /*
  [a b c] [j k l] [aj+bm+cp, ak+bn+cq, al+bo+cr]
  [d e f]*[m n o]=[dj+em+fp, dk+en+fq, dl+eo+fr]
  [g h i] [p q r] [gj+hm+ip, gk+hn+iq, gl+ho+ir]*/
  SMat3 SMat3::operator*(const SMat3& rhs) const {
    SMat3 result;
    result.mbx = MultColumn(*this, rhs.mbx);
    result.mby = MultColumn(*this, rhs.mby);
    result.mbz = MultColumn(*this, rhs.mbz);
    return result;
  }

  SMat3& SMat3::operator*=(const SMat3& rhs) {
    return *this = *this * rhs;
  }

  SFloats SMat3::operator*(SFloats rhs) const {
    return MultColumn(*this, rhs);
  }

  SMat3 SMat3::operator+(const SMat3& rhs) const {
    return SMat3(SAddAll(mbx, rhs.mbx), SAddAll(mby, rhs.mby), SAddAll(mbz, rhs.mbz));
  }

  SMat3& SMat3::operator+=(const SMat3& rhs) {
    mbx = SAddAll(mbx, rhs.mbx);
    mby = SAddAll(mby, rhs.mby);
    mbz = SAddAll(mbz, rhs.mbz);
    return *this;
  }

  SMat3 SMat3::operator-(const SMat3& rhs) const {
    return SMat3(SSubAll(mbx, rhs.mbx), SSubAll(mby, rhs.mby), SSubAll(mbz, rhs.mbz));
  }

  SMat3& SMat3::operator-=(const SMat3& rhs) {
    mbx = SSubAll(mbx, rhs.mbx);
    mby = SSubAll(mby, rhs.mby);
    mbz = SSubAll(mbz, rhs.mbz);
    return *this;
  }

  SMat3 SMat3::Transposed(void) const {
    SMat3 result;
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

  void SMat3::Transpose(void) {
    *this = Transposed();
  }

  SMat3 SMat3::TransposedMultiply(const SMat3& rhs) const {
    return Transposed() * rhs;
  }

  SFloats SMat3::TransposedMultiply(SFloats rhs) const {
    return Transposed() * rhs;
  }

  /*Assume this matrix for comments below:
  [a,b,c]
  [d,e,f]
  [g,h,i]*/
  SMat3 SMat3::Inverse(void) const {
    SMat3 result;
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
    det = SVec3::SafeDivide(SVec3::Identity, det);

    return det * result;
  }

  SMat3 SMat3::Inverse(SFloats det) const {
    SFloats invDet = SVec3::SafeDivide(SVec3::Identity, det);
    SMat3 result;
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

  SFloats SMat3::Determinant(void) const {
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

  SFloats SMat3::ToQuat(void) const {
    //This has so many branches and single scalar operations I don't think it's worth it to try and SIMD it.
    SAlign Mat3 mat;
    SVec3::Store(mbx, mat.mbx);
    SVec3::Store(mby, mat.mby);
    SVec3::Store(mbz, mat.mbz);
    SAlign Quat quat = mat.ToQuat();
    return SLoadAll(&quat.mV.x);
  }

  SMat3 operator*(SFloats lhs, const SMat3& rhs) {
    return SMat3(SMulAll(lhs, rhs.mbx), SMulAll(lhs, rhs.mby), SMulAll(lhs, rhs.mbz));
  }

  SMat3 ToSMat3(const Mat3& mat) {
    return SMat3(ToSVec3(mat.mbx), ToSVec3(mat.mby), ToSVec3(mat.mbz));
  }

  Mat3 ToMat3(const SMat3& mat) {
    return Mat3(ToVec3(mat.mbx), ToVec3(mat.mby), ToVec3(mat.mbz));
  }

  void SMat3::Store(Mat3& store) const {
    SVec3::Store(mbx, store.mbx);
    SVec3::Store(mby, store.mby);
    SVec3::Store(mbz, store.mbz);
  }
}
#endif