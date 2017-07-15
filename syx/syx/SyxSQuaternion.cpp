#include "Precompile.h"
#include "SyxSQuaternion.h"
#include "SyxSVector3.h"
#include "SyxSMatrix3.h"
#include "SyxSIMD.h"

#ifdef SENABLED
namespace Syx {
  const SFloats SQuat::Zero = SLoadFloats(0.0f, 0.0f, 0.0f, 0.0f);

  SFloats SQuat::Construct(SFloats ijk, float w) {
    SAlign Vector3 store;
    SVector3::Store(ijk, store);
    store.w = w;
    return SLoadAll(&store.x);
  }

  /*L=(x,y,z,w), R=(q,r,s,t)
  L and R is just the first 3 elements, not w and t
  w*R + t*L + LcrossR, w*t - LdotR
   [wq+xt+ys-zr]
   [wr+yt+zq-xs]
   [ws+zt+xr-yq]
  -[xq+yr+zs-wt]

   //Last row rearranged to from equation for less operations
   [wt-xq-yr-zs] = wt-(qx+yr+zs) = -(qx+yr+zs)+wt = -(qx+yr+zs-wt)*/
  SFloats SQuat::MulQuat(SFloats lhs, SFloats rhs) {
    SFloats result = SMulAll(SShuffle(lhs, 3, 3, 3, 0), SShuffle(rhs, 0, 1, 2, 0));
    result = SAddAll(result, SMulAll(SShuffle(lhs, 0, 1, 2, 1), SShuffle(rhs, 3, 3, 3, 1)));
    result = SAddAll(result, SMulAll(SShuffle(lhs, 1, 2, 0, 2), SShuffle(rhs, 2, 0, 1, 2)));
    result = SSubAll(result, SMulAll(SShuffle(lhs, 2, 0, 1, 3), SShuffle(rhs, 1, 2, 0, 3)));

    static const SFloats flipWSign = SLoadFloats(1.0f, 1.0f, 1.0f, -1.0f);
    return SMulAll(result, flipWSign);
  }

  SFloats SQuat::MulQuatVec(SFloats quat, SFloats vec) {
    return SMulAll(quat, vec);
  }

  SFloats SQuat::MulVecQuat(SFloats vec, SFloats quat) {
    return SMulAll(vec, quat);
  }

  SFloats SQuat::Add(SFloats lhs, SFloats rhs) {
    return SAddAll(lhs, rhs);
  }

  SFloats SQuat::Div(SFloats lhs, SFloats rhs) {
    return SDivAll(lhs, rhs);
  }

  SFloats SQuat::Neg(SFloats in) {
    static const SFloats negation = SLoadFloats(-1.0f, -1.0f, -1.0f, -1.0f);
    return SMulAll(in, negation);
  }

  //Quaternion multiplication assuming rhs has 0 as its w term
  SFloats QuatMultNoW(SFloats lhs, SFloats rhs) {
    //Same as quaternion multiplication but with t terms removed since it's 0
    SFloats result = SMulAll(SShuffle(lhs, 3, 3, 3, 0), SShuffle(rhs, 0, 1, 2, 0));
    result = SAddAll(result, SMulAll(SShuffle(lhs, 1, 2, 0, 1), SShuffle(rhs, 2, 0, 1, 1)));
    static const SFloats flipWSign = SLoadFloats(1.0f, 1.0f, 1.0f, -1.0f);
    result = SSubAll(result, SMulAll(flipWSign, SMulAll(SShuffle(lhs, 2, 0, 1, 2), SShuffle(rhs, 1, 2, 0, 2))));

    result = SMulAll(result, flipWSign);
    return result;
  }

  SFloats QuatMultZeroW(SFloats lhs, SFloats rhs) {
    SFloats result = SMulAll(SShuffle(lhs, 3, 3, 3, 0), SShuffle(rhs, 0, 1, 2, 0));
    result = SAddAll(result, SMulAll(SShuffle(lhs, 0, 1, 2, 1), SShuffle(rhs, 3, 3, 3, 1)));
    result = SAddAll(result, SMulAll(SShuffle(lhs, 1, 2, 0, 2), SShuffle(rhs, 2, 0, 1, 2)));
    result = SSubAll(result, SMulAll(SShuffle(lhs, 2, 0, 1, 3), SShuffle(rhs, 1, 2, 0, 3)));

    static const SFloats zeroW = SOr(SOr(SVector3::BitsX, SVector3::BitsY), SVector3::BitsZ);
    result = SAnd(result, zeroW);
    return result;
  }

  SFloats SQuat::Rotate(SFloats quat, SFloats toRot) {
    return QuatMultZeroW(QuatMultNoW(quat, toRot), Inversed(quat));
  }

  SFloats SQuat::Length(SFloats in) {
    return SSqrtAll(SVector3::Dot4(in, in));
  }

  SFloats SQuat::Length2(SFloats in) {
    return SVector3::Dot4(in, in);
  }

  SFloats SQuat::Normalized(SFloats in) {
    return SDivAll(in, Length(in));
  }

  SFloats SQuat::Inversed(SFloats in) {
    static const SFloats negation = SLoadFloats(-1.0f, -1.0f, -1.0f, 1.0f);
    return SMulAll(in, negation);
  }

  //-2yy-2zz+1, 2xy-2zw  , 2xz+2yw
  // 2xy+2zw  ,-2xx-2zz+1, 2yz-2xw
  // 2xz-2yw  , 2yz+2xw  ,-2xx-2yy+1
  SMatrix3 SQuat::ToMatrix(SFloats in) {
    static const SFloats allTwo = SLoadFloats(-2.0f, 2.0f, 2.0f, 2.0f);
    SMatrix3 result;

    //Same formula as on standard quaternion, but done per column from left to right as above
    //-2yy-2zz+1
    // 2xy+2zw  
    // 2xz-2yw  
    SFloats cl = allTwo;
    SFloats rhs = SShuffle(in, 1, 0, 0, 3);
    cl = SMulAll(cl, rhs);
    rhs = SShuffle(in, 1, 1, 2, 3);
    cl = SMulAll(cl, rhs);

    SFloats cr = SShuffle(allTwo, 0, 1, 0, 3);
    rhs = SShuffle(in, 2, 2, 1, 3);
    cr = SMulAll(cr, rhs);
    rhs = SShuffle(in, 2, 3, 3, 3);
    cr = SMulAll(cr, rhs);
    cl = SAddAll(cl, cr);
    result.mbx = SAddAll(cl, SVector3::UnitX);

    // 2xy-2zw  
    //-2xx-2zz+1
    // 2yz+2xw  
    cl = SShuffle(allTwo, 1, 0, 1, 3);
    rhs = SShuffle(in, 0, 0, 1, 3);
    cl = SMulAll(cl, rhs);
    rhs = SShuffle(in, 1, 0, 2, 3);
    cl = SMulAll(cl, rhs);

    cr = SShuffle(allTwo, 0, 0, 1, 3);
    rhs = SShuffle(in, 2, 2, 0, 3);
    cr = SMulAll(cr, rhs);
    rhs = SShuffle(in, 3, 2, 3, 3);
    cr = SMulAll(cr, rhs);
    cl = SAddAll(cl, cr);
    result.mby = SAddAll(cl, SVector3::UnitY);

    // 2xz+2yw
    // 2yz-2xw
    //-2xx-2yy+1
    cl = SShuffle(allTwo, 1, 1, 0, 3);
    rhs = SShuffle(in, 0, 1, 0, 3);
    cl = SMulAll(cl, rhs);
    rhs = SShuffle(in, 2, 2, 0, 3);
    cl = SMulAll(cl, rhs);

    cr = SShuffle(allTwo, 1, 0, 0, 3);
    rhs = SShuffle(in, 1, 0, 1, 3);
    cr = SMulAll(cr, rhs);
    rhs = SShuffle(in, 3, 3, 1, 3);
    cr = SMulAll(cr, rhs);
    cl = SAddAll(cl, cr);
    result.mbz = SAddAll(cl, SVector3::UnitZ);
    return result;
  }

  SFloats SQuat::AxisAngle(SFloats axis, float angle) {
    angle *= 0.5f;
    SFloats sinAngle = SLoadSplatFloats(sin(angle));
    return Construct(SMulAll(axis, sinAngle), cos(angle));
  }

  Quat ToQuat(SFloats quat) {
    SAlign Quat store;
    SStoreAll(&store.mV.x, quat);
    return store;
  }

  SFloats ToSQuat(const Quat& quat) {
    AssertAlignment(quat.mV.x);
    return SLoadAll(&quat.mV.x);
  }
}
#endif