#pragma once

#ifdef SENABLED
namespace Syx {
  struct SVec3 {
    static FInline SFloats add(SFloats lhs, SFloats rhs) { return SAddAll(lhs, rhs); }
    static FInline SFloats sub(SFloats lhs, SFloats rhs) { return SSubAll(lhs, rhs); }
    static FInline SFloats neg(SFloats v) { return SSubAll(Zero, v); }
    static FInline SFloats mul(SFloats lhs, SFloats rhs) { return SMulAll(lhs, rhs); }
    static FInline SFloats div(SFloats lhs, SFloats rhs) { return SDivAll(lhs, rhs); }
    static FInline void store(SFloats toStore, Vec3& destination) { SStoreAll(&destination.x, toStore); }
    static FInline SFloats normalized(SFloats in) { return SDivAll(in, length(in)); }
    static FInline SFloats safeNormalized(SFloats in) { return safeDivide(in, length(in)); }
    static FInline SFloats abs(SFloats in) { return sAbsAll(in); }
    static FInline SFloats reciprocal(SFloats in) { return safeDivide(Identity, in); }
    static FInline SFloats length2(SFloats in) { return SVec3::dot(in, in); }
    static FInline SFloats distance(SFloats lhs, SFloats rhs) { return SVec3::length(SVec3::sub(lhs, rhs)); }
    static FInline SFloats distance2(SFloats lhs, SFloats rhs) { return SVec3::length2(SVec3::sub(lhs, rhs)); }
    static FInline SFloats projVec(SFloats vec, SFloats onto) { return mul(onto, div(dot(vec, onto), dot(onto, onto))); }
    static FInline SFloats projVecScalar(SFloats vec, SFloats onto) { return div(dot(vec, onto), dot(onto, onto)); }
    static FInline SFloats lerp(SFloats start, SFloats end, SFloats t) { return add(start, mul(t, sub(end, start))); }
    static FInline SFloats ccwTriangleNormal(SFloats a, SFloats b, SFloats c) { return cross(sub(b, a), sub(c, a)); }
    static FInline SFloats perpendicularLineToPoint(SFloats line, SFloats lineToPoint) { return cross(cross(line, lineToPoint), line); }
    static FInline SFloats barycentricToPoint(const SFloats& a, const SFloats& b, const SFloats& c, const SFloats& ba, const SFloats& bb, const SFloats& bc) { return add(mul(a, ba), add(mul(b, bb), mul(c, bc))); }
    static FInline SFloats pointPlaneProj(SFloats point, SFloats normal, SFloats onPlane) { return sub(point, projVec(sub(point, onPlane), normal)); }
    static FInline SFloats dot(SFloats lhs, SFloats rhs) { return sum3(SMulAll(lhs, rhs)); }
    static FInline SFloats dot4(SFloats lhs, SFloats rhs) { return sum4(SMulAll(lhs, rhs)); }

    //Expensive. Avoid use when possible
    static FInline float get(SFloats in, int index) {
      SAlign Vec3 s;
      store(in, s);
      return s[index];
    }

    //x+y+z into each element
    static FInline SFloats sum3(SFloats in) {
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
    static FInline SFloats mul3(SFloats in) {
      //yxy
      SFloats temp = SShuffle(in, 1, 0, 1, 3);
      SFloats result = SMulAll(in, temp);
      //zzx
      temp = SShuffle(in, 2, 2, 0, 3);
      result = SMulAll(result, temp);
      //Need to put the value in the w element otherwise we get issues with quaternions
      return SShuffle(result, 0, 1, 2, 2);
    }

    static FInline SFloats sum4(SFloats in) {
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

    static FInline SFloats cross(SFloats lhs, SFloats rhs) {
      //[y,z,x]*[z,x,y]
      SFloats lMul = SMulAll(SShuffle(lhs, 1, 2, 0, 3), SShuffle(rhs, 2, 0, 1, 3));
      //[z,x,y]*[y,z,x]
      SFloats rMul = SMulAll(SShuffle(lhs, 2, 0, 1, 3), SShuffle(rhs, 1, 2, 0, 3));
      //[y,z,x]*[z,x,y] - [z,x,y]*[y,z,x]
      return SSubAll(lMul, rMul);
    }

    static FInline SFloats safeDivide(SFloats lhs, SFloats rhs) {
      SFloats mask = SNotEqualAll(rhs, Zero);
      //If it was division by zero the result will mask will be 0, so the result will be cleared
      return SAnd(mask, SDivAll(lhs, rhs));
    }

    static FInline SFloats length(SFloats in) {
      SFloats d = dot(in, in);
      //Put sqrt in lower value
      d = SSqrtLower(d);
      //Splat it all over
      return SShuffle(d, 0, 0, 0, 0);
    }

    //These will have multiple 1 fields if values are the same
    static FInline SFloats leastSignificantAxis(SFloats in) {
      //Compare all agains all others, anding every time, so you end up with all bits set in the right value
      //[x,y,z]<[y,x,x]
      SFloats result = SLessEqualAll(in, SShuffle(in, 1, 0, 0, 3));
      //[x,y,z]<z,z,y]
      result = SAnd(result, SLessEqualAll(in, SShuffle(in, 2, 2, 1, 3)));
      return result;
    }

    static FInline SFloats mostSignificantAxis(SFloats in) {
      //Compare all agains all others, anding every time, so you end up with all bits set in the right value
      //[x,y,z]<[y,x,x]
      SFloats result = SGreaterEqualAll(in, SShuffle(in, 1, 0, 0, 3));
      //[x,y,z]<z,z,y]
      result = SAnd(result, SGreaterEqualAll(in, SShuffle(in, 2, 2, 1, 3)));
      return result;
    }

    static FInline SFloats pointLineDistanceSQ(SFloats point, SFloats start, SFloats end) {
      SFloats projPoint = add(start, projVec(sub(point, start), sub(end, start)));
      return length2(sub(point, projPoint));
    }

    static FInline SFloats equal(SFloats lhs, SFloats rhs, SFloats epsilon = Epsilon) {
      SFloats diff = sAbsAll(SSubAll(lhs, rhs));
      //This will make each component reflect equality with the corresponding one
      SFloats cmp = SLessEqualAll(diff, epsilon);
      //And everything so the result is only true if all components match
       //xyz&yxx
      cmp = SAnd(cmp, SShuffle(cmp, 1, 0, 0, 3));
      //xyz&yxx&zzy
      return SAnd(cmp, SShuffle(cmp, 2, 2, 1, 3));
    }

    static FInline SFloats notEqual(SFloats lhs, SFloats rhs, SFloats epsilon = Epsilon) {
      SFloats diff = sAbsAll(SSubAll(lhs, rhs));
      //This will make each component reflect inequality with the corresponding one
      SFloats cmp = SGreaterAll(diff, epsilon);
      //Or everything so the result is true if any components don't match
       //xyz&yxx
      cmp = SOr(cmp, SShuffle(cmp, 1, 0, 0, 3));
      //xyz&yxx&zzy
      return SOr(cmp, SShuffle(cmp, 2, 2, 1, 3));
    }

    //From Erin Catto's box2d forum
    static FInline void getBasis(SFloats in, SFloats& resultA, SFloats& resultB) {
      // Suppose vector a has all equal components and is a unit vector: a = (s, s, s)
      // Then 3*s*s = 1, s = sqrt(1/3) = 0.57735. This means that at least one component of a
      // unit vector must be greater or equal to 0.57735.
      SAlign float s[4];
      SStoreAll(s, in);
      if(std::abs(s[0]) >= 0.57735f)
        resultA = SVec3::normalized(sLoadFloats(s[1], -s[0], 0.0f));
      else
        resultA = SVec3::normalized(sLoadFloats(0.0f, s[2], -s[1]));

      resultB = cross(in, resultA);
    }

    static FInline SFloats getScalarT(SFloats start, SFloats end, SFloats pointOnLine) {
      SFloats startToEnd = sub(end, start);
      //Slow, should optimize out of this is used often
      SAlign float s[4];
      SStoreAll(s, startToEnd);

      unsigned nonZeroAxis = 0;
      for(unsigned i = 0; i < 3; ++i)
        if(std::abs(s[i]) > SYX_EPSILON) {
          nonZeroAxis = i;
          break;
        }

      SAlign float result = (get(pointOnLine, nonZeroAxis) - get(start, nonZeroAxis))/get(startToEnd, nonZeroAxis);
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

  FInline SFloats toSVec3(const Vec3& vec) {
    AssertAlignment(vec.x)
      return SLoadAll(&vec.x);
  }

  FInline Vec3 toVec3(SFloats vec) {
    SAlign Vec3 result;
    SVec3::store(vec, result);
    return result;
  }

  FInline SFloats toSVec3(SFloats vec) { return vec; }
  FInline Vec3 toVec3(const Vec3& vec) { return vec; }
}
#endif