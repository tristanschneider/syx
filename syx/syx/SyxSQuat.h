#pragma once

#ifdef SENABLED
namespace Syx {
  struct SMat3;

  struct SQuat {
    static SFloats construct(SFloats ijk, float w);

    static SFloats mulQuat(SFloats qa, SFloats qb);
    static SFloats mulQuatVec(SFloats quat, SFloats vec);
    static SFloats mulVecQuat(SFloats vec, SFloats quat);
    static SFloats add(SFloats lhs, SFloats rhs);
    static SFloats div(SFloats lhs, SFloats rhs);
    static SFloats neg(SFloats in);

    static SFloats rotate(SFloats quat, SFloats toRot);

    static SFloats length(SFloats in);
    static SFloats length2(SFloats in);

    static SFloats normalized(SFloats in);
    static SFloats inversed(SFloats in);

    static SMat3 toMatrix(SFloats quat);

    //Construct quaternion from axis angle
    static SFloats 
      axisAngle(SFloats axis, float angle);

    static const SFloats Zero;
  };

  Quat toQuat(SFloats quat);
  SFloats toSQuat(const Quat& quat);
}
#endif