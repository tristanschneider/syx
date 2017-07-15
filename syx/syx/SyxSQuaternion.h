#pragma once
#include "SyxSVector3.h"

#ifdef SENABLED
namespace Syx {
  struct SMatrix3;

  struct SQuat {
    static SFloats SQuat::Construct(SFloats ijk, float w);

    static SFloats MulQuat(SFloats qa, SFloats qb);
    static SFloats MulQuatVec(SFloats quat, SFloats vec);
    static SFloats MulVecQuat(SFloats vec, SFloats quat);
    static SFloats Add(SFloats lhs, SFloats rhs);
    static SFloats Div(SFloats lhs, SFloats rhs);
    static SFloats Neg(SFloats in);

    static SFloats Rotate(SFloats quat, SFloats toRot);

    static SFloats Length(SFloats in);
    static SFloats Length2(SFloats in);

    static SFloats Normalized(SFloats in);
    static SFloats Inversed(SFloats in);

    static SMatrix3 ToMatrix(SFloats quat);

    //Construct quaternion from axis angle
    static SFloats AxisAngle(SFloats axis, float angle);

    static const SFloats Zero;
  };

  Quat ToQuat(SFloats quat);
  SFloats ToSQuat(const Quat& quat);
}
#endif