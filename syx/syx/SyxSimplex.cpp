#include "Precompile.h"
#include "SyxSimplex.h"
#include "SyxNarrowphase.h"

namespace Syx {
  bool Simplex::Add(const SupportPoint& toAdd, bool checkForDuplicates) {
    if(checkForDuplicates)
      for(size_t i = 0; i < mSize; ++i)
        if(mSupports[i].mSupport == toAdd.mSupport)
          return false;

    mSupports[mSize++] = toAdd;
    return true;
  }

  bool Simplex::Contains(const Vector3& support) const {
    for(size_t i = 0; i < mSize; ++i)
      if(mSupports[i].mSupport == support)
        return true;
    return false;
  }

  Vector3 Simplex::Solve(void) {
    mContainsOrigin = false;
    switch(mSize) {
      case 1: return -mSupports[0].mSupport;
      case 2: return SolveLine();
      case 3: return SolveTriangle();
      case 4: return SolveTetrahedron();
    }

    SyxAssertError(false, "Nonsense simplex size");
    return Vector3::Zero;
  }

  void Simplex::Discard(int id) {
    --mSize;
    for(size_t i = static_cast<size_t>(id); i < mSize; ++i)
      mSupports[i] = mSupports[i + 1];
  }

  void Simplex::Discard(int a, int b) {
    if(a > b)
      std::swap(a, b);
    Discard(b);
    Discard(a);
  }

  void Simplex::Discard(int a, int b, int c) {
    OrderAscending(a, b, c);
    Discard(c);
    Discard(b);
    Discard(a);
  }

  void Simplex::FixWinding(void) {
    Vector3 normal = TriangleNormal(Get(SupportID::A), Get(SupportID::B), Get(SupportID::C));
    float dot = normal.Dot(-Get(SupportID::A));
    if(dot > 0.0f)
      std::swap(GetSupport(SupportID::B), GetSupport(SupportID::C));
  }

  Vector3 Simplex::SolveLine(void) {
    const Vector3& a = Get(SupportID::A);
    const Vector3& b = Get(SupportID::B);
    Vector3 bToA = a - b;
    Vector3 bToO = -b;
    float bToALen = bToA.Length2();
    if(std::abs(bToALen) < SYX_EPSILON) {
      mDegenerate = true;
      return Vector3::Zero;
    }

    float t = bToO.Dot(bToA)/bToALen;
    //In front of b
    if(t <= 0.0f) {
      t = 0.0f;
      Discard(SupportID::A);
    }
    else if(t >= 1.0f) {
      t = 1.0f;
      Discard(SupportID::B);
    }

    Vector3 toOrigin = bToO - t*bToA;
    if(toOrigin == Vector3::Zero) {
      mContainsOrigin = true;
      return Vector3::Zero;
    }

    return toOrigin;
  }

  Vector3 Simplex::SolveTriangle(void) {
    //Came from ab, so don't need to check that side
    const Vector3& a = Get(SupportID::A);
    const Vector3& b = Get(SupportID::B);
    const Vector3& c = Get(SupportID::C);

    Vector3 aToB = b - a;
    Vector3 aToC = c - a;
    Vector3 aToO = -a;
    //Result signed areas are (bcp, cap, abp)
    Vector3 bary = PointToBarycentric(aToB, aToC, aToO);
    if(bary == Vector3::Zero) {
      mDegenerate = true;
      return Vector3::Zero;
    }

    if(bary.x <= 0.0f) {
      Discard(SupportID::A);
      return SolveLine();
    }
    if(bary.y <= 0.0f) {
      Discard(SupportID::B);
      return SolveLine();
    }
    if(bary.z <= 0.0f) {
      Discard(SupportID::C);
      return SolveLine();
    }

    Vector3 closestToOrigin = BarycentricToPoint(a, b, c, bary);
    if(closestToOrigin.Length2() < SYX_EPSILON2) {
      mContainsOrigin = true;
      return Vector3::Zero;
    }

    //Point is within triangle, verify winding
    Vector3 normal = aToB.Cross(aToC);
    if(aToO.Dot(normal) > 0.0f)
      std::swap(GetSupport(SupportID::A), GetSupport(SupportID::B));
    return -closestToOrigin;
  }

  //Triangle indices
  struct TriI {
    TriI(int _a, int _b, int _c, int d): a(_a), b(_b), c(_c), unused(d) {}
    int a, b, c, unused;
  };

  //Tetrahedron like this. We came from abc, so don't care about that side
  // B.
  // | C
  // |/   D
  // A
  Vector3 Simplex::SolveTetrahedron(void) {
    //They all need to have D as their last index, because that's the one vertex region it could be in front of
    //bad, cbd, acd
    const TriI tris[3] = {TriI(SupportID::B, SupportID::A, SupportID::D, SupportID::C),
                          TriI(SupportID::C, SupportID::B, SupportID::D, SupportID::A),
                          TriI(SupportID::A, SupportID::C, SupportID::D, SupportID::B)};

    int inFrontDiscard = -1;
    for(int i = 0; i < 3; ++i) {
      const TriI& tri = tris[i];
      const Vector3& a = Get(tri.a);
      const Vector3& b = Get(tri.b);
      const Vector3& c = Get(tri.c);

      //If I adjust the traversal order properly, the neighboring edge calculations can be saved, but that's nitpicking
      Vector3 cToA = a - c;
      Vector3 cToB = b - c;
      Vector3 cToO = -c;
      Vector3 triNormal = cToA.Cross(cToB);

      if(triNormal == Vector3::Zero) {
        mDegenerate = true;
        return Vector3::Zero;
      }

      //If the origin is behind this face, keep going
      if(triNormal.Dot(cToO) > 0.0f) {
        inFrontDiscard = tri.unused;

        //We can only safely discard if triangle contains origin, otherwise we risk destroying triangle that does contain origin
        Vector3 bary = PointToBarycentric(cToA, cToB, cToO);
        if(bary.x >= 0.0f && bary.y >= 0.0f && bary.z >= 0.0f) {
          Discard(inFrontDiscard);
          return SolveTriangle();
        }
      }
    }

    //Will be set if origin was in front of triangle(s) but not contained by any, in which case we can discard the unused vertex of one of those triangles
    if(inFrontDiscard != -1) {
      Discard(inFrontDiscard);
      return SolveTriangle();
    }

    //Not in front of any faces, so must be inside
    mContainsOrigin = true;
    return Vector3::Zero;
  }

  void Simplex::Draw(const Vector3& searchDir) {
    DebugDrawer& dd = DebugDrawer::Get();
    const Vector3& a = Get(SupportID::A);
    const Vector3& b = Get(SupportID::B);
    const Vector3& c = Get(SupportID::C);
    const Vector3& d = Get(SupportID::D);

    dd.SetColor(1.0f, 0.0f, 0.0f);
    float pSize = 0.1f;
    dd.DrawSphere(Vector3::Zero, pSize, Vector3::UnitX, Vector3::UnitY);
    dd.DrawVector(Vector3::Zero, searchDir);

    dd.SetColor(0.0f, 1.0f, 0.0f);
    switch(mSize) {
      case 1:
        dd.DrawPoint(a, pSize);
        break;

      case 2:
        dd.DrawLine(a, b);
        dd.SetColor(0.0f, 0.0f, 1.0f);
        dd.DrawPoint(b, pSize);
        break;

      case 3:
        DrawTriangle(a, b, c, true);
        dd.SetColor(0.0f, 0.0f, 1.0f);
        dd.DrawPoint(c, pSize);
        break;

      case 4:
        DrawTriangle(b, a, d, true);
        DrawTriangle(c, b, d, true);
        DrawTriangle(a, c, d, true);
        dd.SetColor(0.0f, 0.0f, 1.0f);
        DrawTriangle(a, b, c, true);
        dd.DrawPoint(d, pSize);
        break;
    }
  }

  bool Simplex::MakesProgress(const Vector3& newPoint, const Vector3& searchDir) const {
    float newDot = newPoint.Dot(searchDir);
    for(size_t i = 0; i < mSize; ++i)
      if(mSupports[i].mSupport.Dot(searchDir) >= newDot)
        return false;
    return true;
  }

  void Simplex::GrowToFourPoints(Narrowphase& narrow) {
    //Possible directions to look for support points
    static SAlign Vector3 searchDirs[] =
    {
      Vector3::UnitX, Vector3::UnitY, Vector3::UnitZ,
      -Vector3::UnitX, -Vector3::UnitY, -Vector3::UnitZ,
    };

    //Intentional fall through cases
    switch(mSize) {
      //Already has four, get out of here
      case 4:
        return;

      case 0:
        //Arbitrary direction. Doesn't matter since there are no others for it to be a duplicate of
        Add(narrow.GetSupport(Vector3::UnitY), false);

      case 1:
        for(Vector3& curDir : searchDirs) {
          SupportPoint curPoint = narrow.GetSupport(curDir);
          if(curPoint.mSupport.Distance2(mSupports[0].mSupport) > SYX_EPSILON) {
            Add(curPoint, false);
            break;
          }
        }

      case 2:
      {
        //Get closest point to origin on line segment
        Vector3 lineSeg = (mSupports[1].mSupport - mSupports[0].mSupport).SafeNormalized();
        int leastSignificantAxis = lineSeg.LeastSignificantAxis();
        SAlign Vector3 searchDir = lineSeg.Cross(searchDirs[leastSignificantAxis]);
        //Matrix would be a bit faster, but I don't imagine this case comes
        //up often enough for it to matter
        Quat rot = Quat::AxisAngle(lineSeg, 3.14f/3.0f);
        SupportPoint newPoint = narrow.GetSupport(searchDir);

        for(unsigned i = 0; i < 6; ++i) {
          SupportPoint curPoint = narrow.GetSupport(searchDir);
          if(Vector3::PointLineDistanceSQ(curPoint.mSupport, mSupports[0].mSupport, mSupports[1].mSupport) > SYX_EPSILON) {
            newPoint = curPoint;
            break;
          }

          searchDir = rot * searchDir;
        }
        Add(newPoint, false);
      }
      case 3:
      {
        SAlign Vector3 searchDir = TriangleNormal(mSupports[2].mSupport,
          mSupports[1].mSupport, mSupports[0].mSupport);

        SupportPoint newPoint = narrow.GetSupport(searchDir);
        //If this point matches one of the other points already, search in a different direction
        for(unsigned i = 0; i < 3; ++i)
          if(mSupports[i].mSupport == newPoint.mSupport) {
            //For flat shapes this could still result in a duplicate, but we can't do anything better from here
            SAlign Vector3 negDir = -searchDir;
            newPoint = narrow.GetSupport(negDir);
            break;
          }

        Add(newPoint, false);
      }
    }

    //Fix winding
    Vector3 v30 = mSupports[0].mSupport - mSupports[3].mSupport;
    Vector3 v31 = mSupports[1].mSupport - mSupports[3].mSupport;
    Vector3 v32 = mSupports[2].mSupport - mSupports[3].mSupport;
    float det = Vector3::Dot(v30, Vector3::Cross(v31, v32));
    if(det <= 0.0f)
      std::swap(mSupports[0], mSupports[1]);
  }

#ifdef SENABLED
  SFloats Simplex::SSolve(void) {
    mCheckDirection = false;
    SFloats result;
    switch(mSize) {
      case 1: result = SVector3::Neg(SLoadAll(&mSupports[0].mSupport.x)); break;
      case 2: result = SSolveLine(); break;
      case 3: result = SSolveTriangle(); break;
      case 4: result = SSolveTetrahedron(); break;
      default: SyxAssertError(false, "Invalid state encountered in GJK");
        return SVector3::Zero;
    }

    if(mCheckDirection)
      mContainsOrigin = SVector3::Get(SVector3::Equal(result, SVector3::Zero), 0) != 0.0f;
    return result;
  }

  SFloats Simplex::SSolveLine(void) {
    //We came from A, so possible regions are within ab or in front of b
    SFloats a = SLoadAll(&Get(SupportID::A).x);
    SFloats b = SLoadAll(&Get(SupportID::B).x);

    SFloats aToB = SSubAll(b, a);
    SFloats aToO = SVector3::Neg(a);

    SFloats ab2 = SVector3::Dot(aToB, aToB);
    if(SILessLower(ab2, SVector3::Epsilon)) {
      mDegenerate = true;
      return SVector3::Zero;
    }
    SFloats proj = SDivAll(SVector3::Dot(aToO, aToB), ab2);

    //This scalar is 0-1 if within the line segment, otherwise greater than one.
    //It can't be less because we came from B, so we know the origin must be in this direction
    proj = SMinAll(proj, SVector3::Identity);
    //Origin's clamped projection on the line to the origin.
    SFloats result = SVector3::Neg(SAddAll(a, SMulAll(aToB, proj)));

    //Store the result of the projection and the equality so they can both be checked in one unload instead of two
    proj = SShuffle2(proj, SVector3::Equal(result, SVector3::Zero), 0, 0, 0, 0);

    SAlign float unloaded[4];
    SStoreAll(unloaded, proj);

    //If projection was clamped to 1, or was 1, A doesn't contribute to containing the origin
    if(unloaded[0] == 1.0f)
      Discard(SupportID::A);
    //If new search direction is the zero vector
    if(unloaded[2])
      mContainsOrigin = true;

    return result;
  }

  SFloats Simplex::SSolveTriangle(void) {
    SFloats a = SLoadAll(&Get(SupportID::C).x);
    SFloats b = SLoadAll(&Get(SupportID::A).x);
    SFloats c = SLoadAll(&Get(SupportID::B).x);
    SFloats aToC = SSubAll(c, a);
    SFloats aToB = SSubAll(b, a);
    SFloats normal = SVector3::Cross(aToB, aToC);
    SFloats aToO = SVector3::Neg(a);
    int resultRegion = 0;
    SFloats result = SSolveTriangle(aToC, aToB, aToO, normal, a, b, c, resultRegion, true);

    switch(resultRegion) {
      //Vertex C
      case SRegion::A:
        Discard(SupportID::B);
        Discard(SupportID::A);
        break;
        //Line AC
      case SRegion::AB: Discard(SupportID::B); break;
        //Line BC
      case SRegion::AC: Discard(SupportID::A); break;
        //In front of triangle, but fix winding
      case SRegion::FaceToO: std::swap(GetSupport(SupportID::B), GetSupport(SupportID::C)); break;
        //Don't need to do anything for FaceAwayO
    }

    return result;
  }

  //Adapted from Real-Time Collision Detection
  SFloats Simplex::SSolveTriangle(const SFloats& aToC, const SFloats& aToB, const SFloats& aToO, const SFloats& normal,
    const SFloats& a, const SFloats& b, const SFloats& c,
    int& resultRegion, bool checkWinding) {
    if(SVector3::Get(SVector3::Equal(normal, SVector3::Zero), 0)) {
      mDegenerate = true;
      resultRegion = SRegion::FaceAwayO;
      return SVector3::Zero;
    }
    //We came from the ab line, so possible regions are vertex c, line ca,
    //line cb, and in the triangle
    //Using a,b,c as they were used in the book, so line bc becomes where we came from
    SFloats d1 = SVector3::Dot(aToB, aToO);
    SFloats d2 = SVector3::Dot(aToC, aToO);

    //if(d1 <= 0 && d2 <= 0)
    SFloats aRegion = SAnd(SLessEqualAll(d1, SVector3::Zero), SLessEqualAll(d2, SVector3::Zero));

    mCheckDirection = true;
    SFloats bToO = SVector3::Neg(b);
    SFloats cToO = SVector3::Neg(c);
    SFloats d3 = SVector3::Dot(aToB, bToO);
    SFloats d4 = SVector3::Dot(aToC, bToO);
    SFloats d5 = SVector3::Dot(aToB, cToO);
    SFloats d6 = SVector3::Dot(aToC, cToO);

    //Can probably computed acRegion and abRegion at the same time
    //Can probably compute va and vb at the same time
    SFloats vb = SSubAll(SMulAll(d5, d2), SMulAll(d1, d6));
    //if(vb <= 0 && d2 >= 0 && d6 <= 0)
    SFloats acRegion = SAnd(
      SAnd(SLessEqualAll(vb, SVector3::Zero), SGreaterEqualAll(d2, SVector3::Zero)),
      SLessEqualAll(d6, SVector3::Zero));

    SFloats vc = SSubAll(SMulAll(d1, d4), SMulAll(d3, d2));
    //if(vc <= 0 && d1 >= 0 && d3 <= 0)
    SFloats abRegion = SAnd(
      SAnd(SLessEqualAll(vc, SVector3::Zero), SGreaterEqualAll(d1, SVector3::Zero)),
      SLessEqualAll(d3, SVector3::Zero));

    SFloats combined;
    if(!checkWinding)
      combined = SCombine(aRegion, acRegion, abRegion);
    else {
      //Is the normal pointing towards the origin
      SFloats isNToO = SGreaterAll(SVector3::Dot(normal, aToO), SVector3::Zero);
      combined = SCombine(aRegion, acRegion, abRegion, isNToO);
    }
    SAlign float a_ac_ab_n[4];
    SStoreAll(a_ac_ab_n, combined);

    //In front of vertex a
    if(a_ac_ab_n[0]) {
      resultRegion = SRegion::A;
      return aToO;
    }
    //In front of ac line
    if(a_ac_ab_n[1]) {
      resultRegion = SRegion::AC;
      SFloats w = SDivAll(d2, SSubAll(d2, d6));
      return  SVector3::Neg(SAddAll(a, SMulAll(w, aToC)));
    }
    //In front of ab line
    if(a_ac_ab_n[2]) {
      resultRegion = SRegion::AB;
      SFloats v = SDivAll(d1, SSubAll(d1, d3));
      return SVector3::Neg(SAddAll(a, SMulAll(v, aToB)));
    }
    if(SILessLower(SAbsAll(SVector3::Dot(normal, aToO)), SVector3::Epsilon)) {
      mContainsOrigin = true;
      resultRegion = SRegion::FaceAwayO;
      return SVector3::Zero;
    }
    if(!checkWinding) {
      resultRegion = SRegion::Face;
      return normal;
    }
    //Normal facing origin
    if(a_ac_ab_n[3]) {
      resultRegion = SRegion::FaceToO;
      return normal;
    }
    //Normal facing away from origin
    resultRegion = SRegion::FaceAwayO;
    return SVector3::Neg(normal);
  }

  SFloats Simplex::SSolveTetrahedron(void) {
    SFloats a = SLoadAll(&Get(SupportID::A).x);
    SFloats b = SLoadAll(&Get(SupportID::B).x);
    SFloats c = SLoadAll(&Get(SupportID::C).x);
    SFloats d = SLoadAll(&Get(SupportID::D).x);

    //Dot with all faces to figure out which the origin is in front of
    SFloats bToD = SSubAll(d, b);
    SFloats bToA = SSubAll(a, b);
    SFloats bToO = SVector3::Neg(b);
    SFloats badNorm = SVector3::Cross(bToA, bToD);
    SFloats adbRegion = SVector3::Dot(badNorm, bToO);

    SFloats bToC = SSubAll(c, b);
    SFloats bdcNorm = SVector3::Cross(bToD, bToC);
    SFloats bdcRegion = SVector3::Dot(bdcNorm, bToO);

    SFloats aToC = SSubAll(c, a);
    SFloats aToD = SSubAll(d, a);
    SFloats aToO = SVector3::Neg(a);
    SFloats acdNorm = SVector3::Cross(aToC, aToD);
    SFloats acdRegion = SVector3::Dot(acdNorm, aToO);

    SFloats combined = SCombine(bdcRegion, acdRegion, adbRegion);
    SAlign float bdc_acd_adb[4];
    SStoreAll(bdc_acd_adb, combined);

    //Figure out which regions it's in front of, going through all the combinations
    bool bdc = bdc_acd_adb[0] > 0.0f;
    bool acd = bdc_acd_adb[1] > 0.0f;
    bool adb = bdc_acd_adb[2] > 0.0f;
    SFloats result;
    //In front of all faces
    if(bdc && acd && adb)
      result = SThreeTriCase(bdcNorm, acdNorm, badNorm);
    else if(bdc && acd)
      result = STwoTriCase(SupportID::D, SupportID::C, SupportID::B, SupportID::A, bdcNorm, acdNorm);
    else if(bdc && adb)
      result = STwoTriCase(SupportID::D, SupportID::B, SupportID::A, SupportID::C, badNorm, bdcNorm);
    else if(acd && adb)
      result = STwoTriCase(SupportID::D, SupportID::A, SupportID::C, SupportID::B, acdNorm, badNorm);
    else if(bdc)
      result = SOneTriCase(SupportID::D, SupportID::C, SupportID::B, SupportID::A, bdcNorm);
    else if(acd)
      result = SOneTriCase(SupportID::D, SupportID::A, SupportID::C, SupportID::B, acdNorm);
    else if(adb)
      result = SOneTriCase(SupportID::D, SupportID::B, SupportID::A, SupportID::C, badNorm);
    else {
      //Not in front of any face. Collision.
      mContainsOrigin = true;
      mCheckDirection = false;
      result = SVector3::Zero;
    }

    return result;
  }

  SFloats Simplex::SOneTriCase(int a, int b, int c, int d, const SFloats& norm) {
    //For this case the origin is in front of only the abc triangle, so solve for that
    SFloats pointA = SLoadAll(&Get(a).x);
    SFloats pointB = SLoadAll(&Get(b).x);
    SFloats pointC = SLoadAll(&Get(c).x);
    int region;
    //Could go back and reorder region tests so these don't need to be computed, just re-used
    SFloats result = SSolveTriangle(SSubAll(pointC, pointA), SSubAll(pointB, pointA), SVector3::Neg(pointA), norm, pointA, pointB, pointC, region, false);
    SDiscardTetrahedron(a, b, c, d, region);
    return result;
  }

  SFloats Simplex::STwoTriCase(int a, int b, int c, int d, const SFloats& abcNorm, const SFloats& adbNorm) {
    //For two face cases it should either be within one of them or on the line between them,
    //in which case it doesn't matter which closest we pick
    SFloats pointA = SLoadAll(&Get(a).x);
    SFloats pointB = SLoadAll(&Get(b).x);
    SFloats pointC = SLoadAll(&Get(c).x);
    SFloats pointD = SLoadAll(&Get(d).x);

    int abcReg, adbReg;
    SFloats abcResult, adbResult, result;
    abcResult = SSolveTriangle(SSubAll(pointC, pointA), SSubAll(pointB, pointA), SVector3::Neg(pointA), abcNorm, pointA, pointB, pointC, abcReg, false);
    adbResult = SSolveTriangle(SSubAll(pointB, pointA), SSubAll(pointD, pointA), SVector3::Neg(pointA), adbNorm, pointA, pointD, pointB, adbReg, false);
    if(abcReg == SRegion::Face) {
      result = abcResult;
      SDiscardTetrahedron(a, b, c, d, abcReg);
    }
    else {
      result = adbResult;
      SDiscardTetrahedron(a, d, b, c, adbReg);
    }

    return result;
  }

  SFloats Simplex::SThreeTriCase(const SFloats& bdcNorm, const SFloats& acdNorm, const SFloats& badNorm) {
    //In this case, assume that if it's in front of a face, the closest point on that face is best,
    //and if it isn't in front of any face then it must be in the vertex region,
    //in which case all three triangle results are the same
    SFloats a = SLoadAll(&Get(SupportID::A).x);
    SFloats b = SLoadAll(&Get(SupportID::B).x);
    SFloats c = SLoadAll(&Get(SupportID::C).x);
    SFloats d = SLoadAll(&Get(SupportID::D).x);

    int bdcReg, acdReg, adbReg;
    SFloats bdcResult, acdResult, adbResult, result;
    //Should go back and reorder region tests so these don't need to be computed, just re-used
    bdcResult = SSolveTriangle(SSubAll(b, d), SSubAll(c, d), SVector3::Neg(d), bdcNorm, d, c, b, bdcReg, false);
    acdResult = SSolveTriangle(SSubAll(c, d), SSubAll(a, d), SVector3::Neg(d), acdNorm, d, a, c, acdReg, false);
    adbResult = SSolveTriangle(SSubAll(a, d), SSubAll(b, d), SVector3::Neg(d), badNorm, d, b, a, adbReg, false);
    if(bdcReg == SRegion::Face) {
      result = bdcResult;
      SDiscardTetrahedron(SupportID::D, SupportID::C, SupportID::B, SupportID::A, bdcReg);
    }
    else if(acdReg == SRegion::Face) {
      result = acdResult;
      SDiscardTetrahedron(SupportID::D, SupportID::A, SupportID::C, SupportID::B, acdReg);
    }
    else {
      result = adbResult;
      SDiscardTetrahedron(SupportID::D, SupportID::B, SupportID::A, SupportID::C, adbReg);
    }

    return result;
  }

  void Simplex::SDiscardTetrahedron(int, int b, int c, int d, int region) {
    //A is never thrown away in the tetrahedron case
    switch(region) {
      case SRegion::A: Discard(b, c, d); break;
      case SRegion::AB: Discard(c, d); break;
      case SRegion::AC: Discard(b, d); break;
      case SRegion::Face:
        Discard(d);
        FixWinding();
        break;
      case SRegion::FaceAwayO: break;
      default: SyxAssertError(false, "Unhandled region passed to DiscardTetrahedron");
    }
  }

  bool Simplex::SIsDegenerate(void) {
    switch(mSize) {
      case 0:
      case 1:
        return false;
      case 2:
        return Get(SupportID::A) == Get(SupportID::B);
      case 3:
      {
        //Degenerate if triangle normal is zero
        SFloats a = SLoadAll(&Get(SupportID::A).x);
        SFloats b = SLoadAll(&Get(SupportID::B).x);
        SFloats c = SLoadAll(&Get(SupportID::C).x);
        SFloats n = SVector3::Cross(SSubAll(b, a), SSubAll(c, a));
        SFloats nLen = SVector3::Dot(n, n);
        return SILessLower(nLen, SVector3::Epsilon) != 0;
      }
      case 4:
        //Get abc normal and find distance along normal to d. If this is zero then this is degenerate, either because the normal is zero or distance is zero
        SFloats a = SLoadAll(&Get(SupportID::A).x);
        SFloats b = SLoadAll(&Get(SupportID::B).x);
        SFloats c = SLoadAll(&Get(SupportID::C).x);
        SFloats d = SLoadAll(&Get(SupportID::D).x);
        SFloats n = SVector3::Cross(SSubAll(b, a), SSubAll(c, a));
        SFloats dDist = SAbsAll(SVector3::Dot(n, d));
        return SILessLower(dDist, SVector3::Epsilon) != 0;
    }

    //Doesn't matter, shouldn't happen
    return true;
  }

  bool Simplex::SMakesProgress(SFloats newPoint, SFloats searchDir) {
    if(!mSize)
      return true;
    SFloats newDot = SVector3::Dot(searchDir, newPoint);
    SFloats bestDot = SVector3::Dot(searchDir, SLoadAll(&Get(SupportID::A).x));
    //Store the biggest  dot product in bestDot[0]
    for(size_t i = 1; i < mSize; ++i)
      bestDot = SMaxLower(bestDot, SVector3::Dot(searchDir, SLoadAll(&Get(i).x)));

    //If new value is greatest dot product, this is progress
    return SIGreaterLower(newDot, bestDot) != 0;
  }

#else
  SFloats Simplex::SSolve(void) { return SVector3::Zero; }
  SFloats Simplex::SSolveLine(void) { return SVector3::Zero; }
  SFloats Simplex::SSolveTriangle(void) { return SVector3::Zero; }
  SFloats Simplex::SSolveTriangle(const SFloats&, const SFloats&, const SFloats&, const SFloats&, const SFloats&, const SFloats&, const SFloats&, int&, bool) { return SVector3::Zero; }
  SFloats Simplex::SSolveTetrahedron(void) { return SVector3::Zero; }
  void Simplex::SDiscardTetrahedron(int, int, int, int, int) {}
  SFloats Simplex::SOneTriCase(int, int, int, int, const SFloats&) { return SVector3::Zero; }
  SFloats Simplex::STwoTriCase(int, int, int, int, const SFloats&, const SFloats&) { return SVector3::Zero; }
  SFloats Simplex::SThreeTriCase(const SFloats&, const SFloats&, const SFloats&) { return SVector3::Zero; }
  bool Simplex::SIsDegenerate(void) { return false; }
#endif
}