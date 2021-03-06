#include "Precompile.h"

#ifdef SENABLED
namespace Syx {
  const SFloats SQuat::Zero = sLoadFloats(0.0f, 0.0f, 0.0f, 0.0f);

  SFloats SQuat::construct(SFloats ijk, float w) {
    SAlign Vec3 store;
    SVec3::store(ijk, store);
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
  SFloats SQuat::mulQuat(SFloats lhs, SFloats rhs) {
    SFloats result = SMulAll(SShuffle(lhs, 3, 3, 3, 0), SShuffle(rhs, 0, 1, 2, 0));
    result = SAddAll(result, SMulAll(SShuffle(lhs, 0, 1, 2, 1), SShuffle(rhs, 3, 3, 3, 1)));
    result = SAddAll(result, SMulAll(SShuffle(lhs, 1, 2, 0, 2), SShuffle(rhs, 2, 0, 1, 2)));
    result = SSubAll(result, SMulAll(SShuffle(lhs, 2, 0, 1, 3), SShuffle(rhs, 1, 2, 0, 3)));

    static const SFloats flipWSign = sLoadFloats(1.0f, 1.0f, 1.0f, -1.0f);
    return SMulAll(result, flipWSign);
  }

  SFloats SQuat::mulQuatVec(SFloats quat, SFloats vec) {
    return SMulAll(quat, vec);
  }

  SFloats SQuat::mulVecQuat(SFloats vec, SFloats quat) {
    return SMulAll(vec, quat);
  }

  SFloats SQuat::add(SFloats lhs, SFloats rhs) {
    return SAddAll(lhs, rhs);
  }

  SFloats SQuat::div(SFloats lhs, SFloats rhs) {
    return SDivAll(lhs, rhs);
  }

  SFloats SQuat::neg(SFloats in) {
    static const SFloats negation = sLoadFloats(-1.0f, -1.0f, -1.0f, -1.0f);
    return SMulAll(in, negation);
  }

  //Quaternion multiplication assuming rhs has 0 as its w term
  SFloats QuatMultNoW(SFloats lhs, SFloats rhs) {
    //Same as quaternion multiplication but with t terms removed since it's 0
    SFloats result = SMulAll(SShuffle(lhs, 3, 3, 3, 0), SShuffle(rhs, 0, 1, 2, 0));
    result = SAddAll(result, SMulAll(SShuffle(lhs, 1, 2, 0, 1), SShuffle(rhs, 2, 0, 1, 1)));
    static const SFloats flipWSign = sLoadFloats(1.0f, 1.0f, 1.0f, -1.0f);
    result = SSubAll(result, SMulAll(flipWSign, SMulAll(SShuffle(lhs, 2, 0, 1, 2), SShuffle(rhs, 1, 2, 0, 2))));

    result = SMulAll(result, flipWSign);
    return result;
  }

  SFloats QuatMultZeroW(SFloats lhs, SFloats rhs) {
    SFloats result = SMulAll(SShuffle(lhs, 3, 3, 3, 0), SShuffle(rhs, 0, 1, 2, 0));
    result = SAddAll(result, SMulAll(SShuffle(lhs, 0, 1, 2, 1), SShuffle(rhs, 3, 3, 3, 1)));
    result = SAddAll(result, SMulAll(SShuffle(lhs, 1, 2, 0, 2), SShuffle(rhs, 2, 0, 1, 2)));
    result = SSubAll(result, SMulAll(SShuffle(lhs, 2, 0, 1, 3), SShuffle(rhs, 1, 2, 0, 3)));

    static const SFloats zeroW = SOr(SOr(SVec3::BitsX, SVec3::BitsY), SVec3::BitsZ);
    result = SAnd(result, zeroW);
    return result;
  }

  SFloats SQuat::rotate(SFloats quat, SFloats toRot) {
    return QuatMultZeroW(QuatMultNoW(quat, toRot), inversed(quat));
  }

  SFloats SQuat::length(SFloats in) {
    return SSqrtAll(SVec3::dot4(in, in));
  }

  SFloats SQuat::length2(SFloats in) {
    return SVec3::dot4(in, in);
  }

  SFloats SQuat::normalized(SFloats in) {
    return SDivAll(in, length(in));
  }

  SFloats SQuat::inversed(SFloats in) {
    static const SFloats negation = sLoadFloats(-1.0f, -1.0f, -1.0f, 1.0f);
    return SMulAll(in, negation);
  }

  //-2yy-2zz+1, 2xy-2zw  , 2xz+2yw
  // 2xy+2zw  ,-2xx-2zz+1, 2yz-2xw
  // 2xz-2yw  , 2yz+2xw  ,-2xx-2yy+1
  SMat3 SQuat::toMatrix(SFloats in) {
    static const SFloats allTwo = sLoadFloats(-2.0f, 2.0f, 2.0f, 2.0f);
    SMat3 result;

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
    result.mbx = SAddAll(cl, SVec3::UnitX);

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
    result.mby = SAddAll(cl, SVec3::UnitY);

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
    result.mbz = SAddAll(cl, SVec3::UnitZ);
    return result;
  }

  SFloats SQuat::axisAngle(SFloats axis, float angle) {
    angle *= 0.5f;
    SFloats sinAngle = sLoadSplatFloats(sin(angle));
    return construct(SMulAll(axis, sinAngle), cos(angle));
  }

  Quat toQuat(SFloats quat) {
    SAlign Quat store;
    SStoreAll(&store.mV.x, quat);
    return store;
  }

  SFloats toSQuat(const Quat& quat) {
    AssertAlignment(quat.mV.x);
    return SLoadAll(&quat.mV.x);
  }
}
#endif