#pragma once
#include "SyxSIMD.h"
#include "SyxMathDefines.h"

#ifdef SENABLED
namespace Syx {
  struct Vector3;

  struct SVector3 {
    static FInline SFloats Add(SFloats lhs, SFloats rhs) { return SAddAll(lhs, rhs); }
    static FInline SFloats Sub(SFloats lhs, SFloats rhs) { return SSubAll(lhs, rhs); }
    static FInline SFloats Neg(SFloats v) { return SSubAll(Zero, v); }
    static FInline SFloats Mul(SFloats lhs, SFloats rhs) { return SMulAll(lhs, rhs); }
    static FInline SFloats Div(SFloats lhs, SFloats rhs) { return SDivAll(lhs, rhs); }
    static FInline void Store(SFloats toStore, Vector3& destination) { SStoreAll(&destination.x, toStore); }
    static FInline SFloats Normalized(SFloats in) { return SDivAll(in, Length(in)); }
    static FInline SFloats SafeNormalized(SFloats in) { return SafeDivide(in, Length(in)); }
    static FInline SFloats Abs(SFloats in) { return SAbsAll(in); }
    static FInline SFloats Reciprocal(SFloats in) { return SafeDivide(Identity, in); }
    static FInline SFloats Length2(SFloats in) { return SVector3::Dot(in, in); }
    static FInline SFloats Distance(SFloats lhs, SFloats rhs) { return SVector3::Length(SVector3::Sub(lhs, rhs)); }
    static FInline SFloats Distance2(SFloats lhs, SFloats rhs) { return SVector3::Length2(SVector3::Sub(lhs, rhs)); }
    static FInline SFloats ProjVec(SFloats vec, SFloats onto) { return Mul(onto, Div(Dot(vec, onto), Dot(onto, onto))); }
    static FInline SFloats ProjVecScalar(SFloats vec, SFloats onto) { return Div(Dot(vec, onto), Dot(onto, onto)); }
    static FInline SFloats Lerp(SFloats start, SFloats end, SFloats t) { return Add(start, Mul(t, Sub(end, start))); }
    static FInline SFloats CCWTriangleNormal(SFloats a, SFloats b, SFloats c) { return Cross(Sub(b, a), Sub(c, a)); }
    static FInline SFloats PerpendicularLineToPoint(SFloats line, SFloats lineToPoint) { return Cross(Cross(line, lineToPoint), line); }
    static FInline SFloats BarycentricToPoint(const SFloats& a, const SFloats& b, const SFloats& c, const SFloats& ba, const SFloats& bb, const SFloats& bc) { return Add(Mul(a, ba), Add(Mul(b, bb), Mul(c, bc))); }
    static FInline SFloats PointPlaneProj(SFloats point, SFloats normal, SFloats onPlane) { return Sub(point, ProjVec(Sub(point, onPlane), normal)); }
    static FInline SFloats Dot(SFloats lhs, SFloats rhs) { return Sum3(SMulAll(lhs, rhs)); }
    static FInline SFloats Dot4(SFloats lhs, SFloats rhs) { return Sum4(SMulAll(lhs, rhs)); }

    //Expensive. Avoid use when possible
    static FInline float Get(SFloats in, int index) {
      SAlign Vector3 store;
      Store(in, store);
      return store[index];
    }

    //x+y+z into each element
    static FInline SFloats Sum3(SFloats in) {
      //yxy
      SFloats temp = SShuffle(in, 1, 0, 1, 3);
      SFloats result = SAddAll(in, temp);
      //zzx
      temp = SShuffle(in, 2, 2, 0, 3);
      result = SAddAll(result, temp);
      //Need to put the value in the w element otherwise we get issues with quaternions
      return SShuffle(result, 0, 1, 2, 2);
    }

    //x*y*z into eaach element
    static FInline SFloats Mul3(SFloats in) {
      //yxy
      SFloats temp = SShuffle(in, 1, 0, 1, 3);
      SFloats result = SMulAll(in, temp);
      //zzx
      temp = SShuffle(in, 2, 2, 0, 3);
      result = SMulAll(result, temp);
      //Need to put the value in the w element otherwise we get issues with quaternions
      return SShuffle(result, 0, 1, 2, 2);
    }

    static FInline SFloats Sum4(SFloats in) {
      //yxyz
      SFloats temp = SShuffle(in, 1, 0, 1, 2);
      SFloats result = SAddAll(in, temp);
      //zzxy
      temp = SShuffle(in, 2, 2, 0, 1);
      result = SAddAll(result, temp);
      //wwwx
      temp = SShuffle(in, 3, 3, 3, 0);
      return SAddAll(result, temp);
    }

    static FInline SFloats Cross(SFloats lhs, SFloats rhs) {
      //[y,z,x]*[z,x,y]
      SFloats lMul = SMulAll(SShuffle(lhs, 1, 2, 0, 3), SShuffle(rhs, 2, 0, 1, 3));
      //[z,x,y]*[y,z,x]
      SFloats rMul = SMulAll(SShuffle(lhs, 2, 0, 1, 3), SShuffle(rhs, 1, 2, 0, 3));
      //[y,z,x]*[z,x,y] - [z,x,y]*[y,z,x]
      return SSubAll(lMul, rMul);
    }

    static FInline SFloats SafeDivide(SFloats lhs, SFloats rhs) {
      SFloats mask = SNotEqualAll(rhs, Zero);
      //If it was division by zero the result will mask will be 0, so the result will be cleared
      return SAnd(mask, SDivAll(lhs, rhs));
    }

    static FInline SFloats Length(SFloats in) {
      SFloats dot = Dot(in, in);
      //Put sqrt in lower value
      dot = SSqrtLower(dot);
      //Splat it all over
      return SShuffle(dot, 0, 0, 0, 0);
    }

    //These will have multiple 1 fields if values are the same
    static FInline SFloats LeastSignificantAxis(SFloats in) {
      //Compare all agains all others, anding every time, so you end up with all bits set in the right value
      //[x,y,z]<[y,x,x]
      SFloats result = SLessEqualAll(in, SShuffle(in, 1, 0, 0, 3));
      //[x,y,z]<z,z,y]
      result = SAnd(result, SLessEqualAll(in, SShuffle(in, 2, 2, 1, 3)));
      return result;
    }

    static FInline SFloats MostSignificantAxis(SFloats in) {
      //Compare all agains all others, anding every time, so you end up with all bits set in the right value
      //[x,y,z]<[y,x,x]
      SFloats result = SGreaterEqualAll(in, SShuffle(in, 1, 0, 0, 3));
      //[x,y,z]<z,z,y]
      result = SAnd(result, SGreaterEqualAll(in, SShuffle(in, 2, 2, 1, 3)));
      return result;
    }

    static FInline SFloats PointLineDistanceSQ(SFloats point, SFloats start, SFloats end) {
      SFloats projPoint = Add(start, ProjVec(Sub(point, start), Sub(end, start)));
      return Length2(Sub(point, projPoint));
    }

    static FInline SFloats Equal(SFloats lhs, SFloats rhs, SFloats epsilon = Epsilon) {
      SFloats diff = SAbsAll(SSubAll(lhs, rhs));
      //This will make each component reflect equality with the corresponding one
      SFloats cmp = SLessEqualAll(diff, epsilon);
      //And everything so the result is only true if all components match
       //xyz&yxx
      cmp = SAnd(cmp, SShuffle(cmp, 1, 0, 0, 3));
      //xyz&yxx&zzy
      return SAnd(cmp, SShuffle(cmp, 2, 2, 1, 3));
    }

    static FInline SFloats NotEqual(SFloats lhs, SFloats rhs, SFloats epsilon = Epsilon) {
      SFloats diff = SAbsAll(SSubAll(lhs, rhs));
      //This will make each component reflect inequality with the corresponding one
      SFloats cmp = SGreaterAll(diff, epsilon);
      //Or everything so the result is true if any components don't match
       //xyz&yxx
      cmp = SOr(cmp, SShuffle(cmp, 1, 0, 0, 3));
      //xyz&yxx&zzy
      return SOr(cmp, SShuffle(cmp, 2, 2, 1, 3));
    }

    //From Erin Catto's box2d forum
    static FInline void GetBasis(SFloats in, SFloats& resultA, SFloats& resultB) {
      // Suppose vector a has all equal components and is a unit vector: a = (s, s, s)
      // Then 3*s*s = 1, s = sqrt(1/3) = 0.57735. This means that at least one component of a
      // unit vector must be greater or equal to 0.57735.
      SAlign float store[4];
      SStoreAll(store, in);
      if(abs(store[0]) >= 0.57735f)
        resultA = SVector3::Normalized(SLoadFloats(store[1], -store[0], 0.0f));
      else
        resultA = SVector3::Normalized(SLoadFloats(0.0f, store[2], -store[1]));

      resultB = Cross(in, resultA);
    }

    static FInline SFloats GetScalarT(SFloats start, SFloats end, SFloats pointOnLine) {
      SFloats startToEnd = Sub(end, start);
      //Slow, should optimize out of this is used often
      SAlign float store[4];
      SStoreAll(store, startToEnd);

      unsigned nonZeroAxis = 0;
      for(unsigned i = 0; i < 3; ++i)
        if(abs(store[i]) > SYX_EPSILON) {
          nonZeroAxis = i;
          break;
        }

      SAlign float result = (Get(pointOnLine, nonZeroAxis) - Get(start, nonZeroAxis))/Get(startToEnd, nonZeroAxis);
      return SLoadSplat(&result);
    }

    const static SFloats UnitX;
    const static SFloats UnitY;
    const static SFloats UnitZ;
    const static SFloats Zero;
    const static SFloats Identity;
    const static SFloats Epsilon;
    const static SFloats BitsX;
    const static SFloats BitsY;
    const static SFloats BitsZ;
    const static SFloats BitsW;
    const static SFloats BitsAll;
  };

  FInline SFloats ToSVector3(const Vector3& vec) {
    AssertAlignment(vec.x)
      return SLoadAll(&vec.x);
  }

  FInline Vector3 ToVector3(SFloats vec) {
    SAlign Vector3 result;
    SVector3::Store(vec, result);
    return result;
  }

  FInline SFloats ToSVector3(SFloats vec) { return vec; }
  FInline Vector3 ToVector3(const Vector3& vec) { return vec; }
}
#endif