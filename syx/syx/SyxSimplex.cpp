#include "Precompile.h"
#include "SyxSimplex.h"
#include "SyxNarrowphase.h"

namespace Syx {
  bool Simplex::add(const SupportPoint& toAdd, bool checkForDuplicates) {
    if(checkForDuplicates)
      for(size_t i = 0; i < mSize; ++i)
        if(mSupports[i].mSupport == toAdd.mSupport)
          return false;

    mSupports[mSize++] = toAdd;
    return true;
  }

  bool Simplex::contains(const Vec3& support) const {
    for(size_t i = 0; i < mSize; ++i)
      if(mSupports[i].mSupport == support)
        return true;
    return false;
  }

  Vec3 Simplex::solve(void) {
    mContainsOrigin = false;
    switch(mSize) {
      case 1: return -mSupports[0].mSupport;
      case 2: return _solveLine();
      case 3: return _solveTriangle();
      case 4: return _solveTetrahedron();
    }

    SyxAssertError(false, "Nonsense simplex size");
    return Vec3::Zero;
  }

  void Simplex::_discard(int id) {
    --mSize;
    for(size_t i = static_cast<size_t>(id); i < mSize; ++i)
      mSupports[i] = mSupports[i + 1];
  }

  void Simplex::_discard(int a, int b) {
    if(a > b)
      std::swap(a, b);
    _discard(b);
    _discard(a);
  }

  void Simplex::_discard(int a, int b, int c) {
    orderAscending(a, b, c);
    _discard(c);
    _discard(b);
    _discard(a);
  }

  void Simplex::_fixWinding(void) {
    Vec3 normal = triangleNormal(get(SupportID::A), get(SupportID::B), get(SupportID::C));
    float dot = normal.dot(-get(SupportID::A));
    if(dot > 0.0f)
      std::swap(getSupport(SupportID::B), getSupport(SupportID::C));
  }

  Vec3 Simplex::_solveLine(void) {
    const Vec3& a = get(SupportID::A);
    const Vec3& b = get(SupportID::B);
    Vec3 bToA = a - b;
    Vec3 bToO = -b;
    float bToALen = bToA.length2();
    if(std::abs(bToALen) < SYX_EPSILON) {
      mDegenerate = true;
      return Vec3::Zero;
    }

    float t = bToO.dot(bToA)/bToALen;
    //In front of b
    if(t <= 0.0f) {
      t = 0.0f;
      _discard(SupportID::A);
    }
    else if(t >= 1.0f) {
      t = 1.0f;
      _discard(SupportID::B);
    }

    Vec3 toOrigin = bToO - t*bToA;
    if(toOrigin == Vec3::Zero) {
      mContainsOrigin = true;
      return Vec3::Zero;
    }

    return toOrigin;
  }

  Vec3 Simplex::_solveTriangle(void) {
    //Came from ab, so don't need to check that side
    const Vec3& a = get(SupportID::A);
    const Vec3& b = get(SupportID::B);
    const Vec3& c = get(SupportID::C);

    Vec3 aToB = b - a;
    Vec3 aToC = c - a;
    Vec3 aToO = -a;
    //Result signed areas are (bcp, cap, abp)
    Vec3 bary = pointToBarycentric(aToB, aToC, aToO);
    if(bary == Vec3::Zero) {
      mDegenerate = true;
      return Vec3::Zero;
    }

    if(bary.x <= 0.0f) {
      _discard(SupportID::A);
      return _solveLine();
    }
    if(bary.y <= 0.0f) {
      _discard(SupportID::B);
      return _solveLine();
    }
    if(bary.z <= 0.0f) {
      _discard(SupportID::C);
      return _solveLine();
    }

    Vec3 closestToOrigin = barycentricToPoint(a, b, c, bary);
    if(closestToOrigin.length2() < SYX_EPSILON2) {
      mContainsOrigin = true;
      return Vec3::Zero;
    }

    //Point is within triangle, verify winding
    Vec3 normal = aToB.cross(aToC);
    if(aToO.dot(normal) > 0.0f)
      std::swap(getSupport(SupportID::A), getSupport(SupportID::B));
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
  Vec3 Simplex::_solveTetrahedron(void) {
    //They all need to have D as their last index, because that's the one vertex region it could be in front of
    //bad, cbd, acd
    const TriI tris[3] = {TriI(SupportID::B, SupportID::A, SupportID::D, SupportID::C),
                          TriI(SupportID::C, SupportID::B, SupportID::D, SupportID::A),
                          TriI(SupportID::A, SupportID::C, SupportID::D, SupportID::B)};

    int inFrontDiscard = -1;
    for(int i = 0; i < 3; ++i) {
      const TriI& tri = tris[i];
      const Vec3& a = get(tri.a);
      const Vec3& b = get(tri.b);
      const Vec3& c = get(tri.c);

      //If I adjust the traversal order properly, the neighboring edge calculations can be saved, but that's nitpicking
      Vec3 cToA = a - c;
      Vec3 cToB = b - c;
      Vec3 cToO = -c;
      Vec3 triNormal = cToA.cross(cToB);

      if(triNormal == Vec3::Zero) {
        mDegenerate = true;
        return Vec3::Zero;
      }

      //If the origin is behind this face, keep going
      if(triNormal.dot(cToO) > 0.0f) {
        inFrontDiscard = tri.unused;

        //We can only safely discard if triangle contains origin, otherwise we risk destroying triangle that does contain origin
        Vec3 bary = pointToBarycentric(cToA, cToB, cToO);
        if(bary.x >= 0.0f && bary.y >= 0.0f && bary.z >= 0.0f) {
          _discard(inFrontDiscard);
          return _solveTriangle();
        }
      }
    }

    //Will be set if origin was in front of triangle(s) but not contained by any, in which case we can discard the unused vertex of one of those triangles
    if(inFrontDiscard != -1) {
      _discard(inFrontDiscard);
      return _solveTriangle();
    }

    //Not in front of any faces, so must be inside
    mContainsOrigin = true;
    return Vec3::Zero;
  }

  void Simplex::draw(const Vec3& searchDir) {
    DebugDrawer& dd = DebugDrawer::get();
    const Vec3& a = get(SupportID::A);
    const Vec3& b = get(SupportID::B);
    const Vec3& c = get(SupportID::C);
    const Vec3& d = get(SupportID::D);

    dd.setColor(1.0f, 0.0f, 0.0f);
    float pSize = 0.1f;
    dd.drawSphere(Vec3::Zero, pSize, Vec3::UnitX, Vec3::UnitY);
    dd.drawVector(Vec3::Zero, searchDir);

    dd.setColor(0.0f, 1.0f, 0.0f);
    switch(mSize) {
      case 1:
        dd.drawPoint(a, pSize);
        break;

      case 2:
        dd.drawLine(a, b);
        dd.setColor(0.0f, 0.0f, 1.0f);
        dd.drawPoint(b, pSize);
        break;

      case 3:
        drawTriangle(a, b, c, true);
        dd.setColor(0.0f, 0.0f, 1.0f);
        dd.drawPoint(c, pSize);
        break;

      case 4:
        drawTriangle(b, a, d, true);
        drawTriangle(c, b, d, true);
        drawTriangle(a, c, d, true);
        dd.setColor(0.0f, 0.0f, 1.0f);
        drawTriangle(a, b, c, true);
        dd.drawPoint(d, pSize);
        break;
    }
  }

  bool Simplex::makesProgress(const Vec3& newPoint, const Vec3& searchDir) const {
    float newDot = newPoint.dot(searchDir);
    for(size_t i = 0; i < mSize; ++i)
      if(mSupports[i].mSupport.dot(searchDir) >= newDot)
        return false;
    return true;
  }

  void Simplex::growToFourPoints(Narrowphase& narrow) {
    //Possible directions to look for support points
    static SAlign Vec3 searchDirs[] =
    {
      Vec3::UnitX, Vec3::UnitY, Vec3::UnitZ,
      -Vec3::UnitX, -Vec3::UnitY, -Vec3::UnitZ,
    };

    //Intentional fall through cases
    switch(mSize) {
      //Already has four, get out of here
      case 4:
        return;

      case 0:
        //Arbitrary direction. Doesn't matter since there are no others for it to be a duplicate of
        add(narrow._getSupport(Vec3::UnitY), false);

      case 1:
        for(Vec3& curDir : searchDirs) {
          SupportPoint curPoint = narrow._getSupport(curDir);
          if(curPoint.mSupport.distance2(mSupports[0].mSupport) > SYX_EPSILON) {
            add(curPoint, false);
            break;
          }
        }

      case 2:
      {
        //Get closest point to origin on line segment
        Vec3 lineSeg = (mSupports[1].mSupport - mSupports[0].mSupport).safeNormalized();
        int leastSignificantAxis = lineSeg.leastSignificantAxis();
        SAlign Vec3 searchDir = lineSeg.cross(searchDirs[leastSignificantAxis]);
        //Matrix would be a bit faster, but I don't imagine this case comes
        //up often enough for it to matter
        Quat rot = Quat::axisAngle(lineSeg, 3.14f/3.0f);
        SupportPoint newPoint = narrow._getSupport(searchDir);

        for(unsigned i = 0; i < 6; ++i) {
          SupportPoint curPoint = narrow._getSupport(searchDir);
          if(Vec3::pointLineDistanceSQ(curPoint.mSupport, mSupports[0].mSupport, mSupports[1].mSupport) > SYX_EPSILON) {
            newPoint = curPoint;
            break;
          }

          searchDir = rot * searchDir;
        }
        add(newPoint, false);
      }
      case 3:
      {
        SAlign Vec3 searchDir = triangleNormal(mSupports[2].mSupport,
          mSupports[1].mSupport, mSupports[0].mSupport);

        SupportPoint newPoint = narrow._getSupport(searchDir);
        //If this point matches one of the other points already, search in a different direction
        for(unsigned i = 0; i < 3; ++i)
          if(mSupports[i].mSupport == newPoint.mSupport) {
            //For flat shapes this could still result in a duplicate, but we can't do anything better from here
            SAlign Vec3 negDir = -searchDir;
            newPoint = narrow._getSupport(negDir);
            break;
          }

        add(newPoint, false);
      }
    }

    //Fix winding
    Vec3 v30 = mSupports[0].mSupport - mSupports[3].mSupport;
    Vec3 v31 = mSupports[1].mSupport - mSupports[3].mSupport;
    Vec3 v32 = mSupports[2].mSupport - mSupports[3].mSupport;
    float det = Vec3::dot(v30, Vec3::cross(v31, v32));
    if(det <= 0.0f)
      std::swap(mSupports[0], mSupports[1]);
  }

#ifdef SENABLED
  SFloats Simplex::sSolve(void) {
    mCheckDirection = false;
    SFloats result;
    switch(mSize) {
      case 1: result = SVec3::neg(SLoadAll(&mSupports[0].mSupport.x)); break;
      case 2: result = _sSolveLine(); break;
      case 3: result = _sSolveTriangle(); break;
      case 4: result = _sSolveTetrahedron(); break;
      default: SyxAssertError(false, "Invalid state encountered in GJK");
        return SVec3::Zero;
    }

    if(mCheckDirection)
      mContainsOrigin = SVec3::get(SVec3::equal(result, SVec3::Zero), 0) != 0.0f;
    return result;
  }

  SFloats Simplex::_sSolveLine(void) {
    //We came from A, so possible regions are within ab or in front of b
    SFloats a = SLoadAll(&get(SupportID::A).x);
    SFloats b = SLoadAll(&get(SupportID::B).x);

    SFloats aToB = SSubAll(b, a);
    SFloats aToO = SVec3::neg(a);

    SFloats ab2 = SVec3::dot(aToB, aToB);
    if(SILessLower(ab2, SVec3::Epsilon)) {
      mDegenerate = true;
      return SVec3::Zero;
    }
    SFloats proj = SDivAll(SVec3::dot(aToO, aToB), ab2);

    //This scalar is 0-1 if within the line segment, otherwise greater than one.
    //It can't be less because we came from B, so we know the origin must be in this direction
    proj = SMinAll(proj, SVec3::Identity);
    //Origin's clamped projection on the line to the origin.
    SFloats result = SVec3::neg(SAddAll(a, SMulAll(aToB, proj)));

    //Store the result of the projection and the equality so they can both be checked in one unload instead of two
    proj = SShuffle2(proj, SVec3::equal(result, SVec3::Zero), 0, 0, 0, 0);

    SAlign float unloaded[4];
    SStoreAll(unloaded, proj);

    //If projection was clamped to 1, or was 1, A doesn't contribute to containing the origin
    if(unloaded[0] == 1.0f)
      _discard(SupportID::A);
    //If new search direction is the zero vector
    if(unloaded[2])
      mContainsOrigin = true;

    return result;
  }

  SFloats Simplex::_sSolveTriangle(void) {
    SFloats a = SLoadAll(&get(SupportID::C).x);
    SFloats b = SLoadAll(&get(SupportID::A).x);
    SFloats c = SLoadAll(&get(SupportID::B).x);
    SFloats aToC = SSubAll(c, a);
    SFloats aToB = SSubAll(b, a);
    SFloats normal = SVec3::cross(aToB, aToC);
    SFloats aToO = SVec3::neg(a);
    int resultRegion = 0;
    SFloats result = _sSolveTriangle(aToC, aToB, aToO, normal, a, b, c, resultRegion, true);

    switch(resultRegion) {
      //Vertex C
      case SRegion::A:
        _discard(SupportID::B);
        _discard(SupportID::A);
        break;
        //Line AC
      case SRegion::AB: _discard(SupportID::B); break;
        //Line BC
      case SRegion::AC: _discard(SupportID::A); break;
        //In front of triangle, but fix winding
      case SRegion::FaceToO: std::swap(getSupport(SupportID::B), getSupport(SupportID::C)); break;
        //Don't need to do anything for FaceAwayO
    }

    return result;
  }

  //Adapted from Real-Time Collision Detection
  SFloats Simplex::_sSolveTriangle(const SFloats& aToC, const SFloats& aToB, const SFloats& aToO, const SFloats& normal,
    const SFloats& a, const SFloats& b, const SFloats& c,
    int& resultRegion, bool checkWinding) {
    if(SVec3::get(SVec3::equal(normal, SVec3::Zero), 0)) {
      mDegenerate = true;
      resultRegion = SRegion::FaceAwayO;
      return SVec3::Zero;
    }
    //We came from the ab line, so possible regions are vertex c, line ca,
    //line cb, and in the triangle
    //Using a,b,c as they were used in the book, so line bc becomes where we came from
    SFloats d1 = SVec3::dot(aToB, aToO);
    SFloats d2 = SVec3::dot(aToC, aToO);

    //if(d1 <= 0 && d2 <= 0)
    SFloats aRegion = SAnd(SLessEqualAll(d1, SVec3::Zero), SLessEqualAll(d2, SVec3::Zero));

    mCheckDirection = true;
    SFloats bToO = SVec3::neg(b);
    SFloats cToO = SVec3::neg(c);
    SFloats d3 = SVec3::dot(aToB, bToO);
    SFloats d4 = SVec3::dot(aToC, bToO);
    SFloats d5 = SVec3::dot(aToB, cToO);
    SFloats d6 = SVec3::dot(aToC, cToO);

    //Can probably computed acRegion and abRegion at the same time
    //Can probably compute va and vb at the same time
    SFloats vb = SSubAll(SMulAll(d5, d2), SMulAll(d1, d6));
    //if(vb <= 0 && d2 >= 0 && d6 <= 0)
    SFloats acRegion = SAnd(
      SAnd(SLessEqualAll(vb, SVec3::Zero), SGreaterEqualAll(d2, SVec3::Zero)),
      SLessEqualAll(d6, SVec3::Zero));

    SFloats vc = SSubAll(SMulAll(d1, d4), SMulAll(d3, d2));
    //if(vc <= 0 && d1 >= 0 && d3 <= 0)
    SFloats abRegion = SAnd(
      SAnd(SLessEqualAll(vc, SVec3::Zero), SGreaterEqualAll(d1, SVec3::Zero)),
      SLessEqualAll(d3, SVec3::Zero));

    SFloats combined;
    if(!checkWinding)
      combined = sCombine(aRegion, acRegion, abRegion);
    else {
      //Is the normal pointing towards the origin
      SFloats isNToO = SGreaterAll(SVec3::dot(normal, aToO), SVec3::Zero);
      combined = sCombine(aRegion, acRegion, abRegion, isNToO);
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
      return  SVec3::neg(SAddAll(a, SMulAll(w, aToC)));
    }
    //In front of ab line
    if(a_ac_ab_n[2]) {
      resultRegion = SRegion::AB;
      SFloats v = SDivAll(d1, SSubAll(d1, d3));
      return SVec3::neg(SAddAll(a, SMulAll(v, aToB)));
    }
    if(SILessLower(sAbsAll(SVec3::dot(normal, aToO)), SVec3::Epsilon)) {
      mContainsOrigin = true;
      resultRegion = SRegion::FaceAwayO;
      return SVec3::Zero;
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
    return SVec3::neg(normal);
  }

  SFloats Simplex::_sSolveTetrahedron(void) {
    SFloats a = SLoadAll(&get(SupportID::A).x);
    SFloats b = SLoadAll(&get(SupportID::B).x);
    SFloats c = SLoadAll(&get(SupportID::C).x);
    SFloats d = SLoadAll(&get(SupportID::D).x);

    //Dot with all faces to figure out which the origin is in front of
    SFloats bToD = SSubAll(d, b);
    SFloats bToA = SSubAll(a, b);
    SFloats bToO = SVec3::neg(b);
    SFloats badNorm = SVec3::cross(bToA, bToD);
    SFloats adbRegion = SVec3::dot(badNorm, bToO);

    SFloats bToC = SSubAll(c, b);
    SFloats bdcNorm = SVec3::cross(bToD, bToC);
    SFloats bdcRegion = SVec3::dot(bdcNorm, bToO);

    SFloats aToC = SSubAll(c, a);
    SFloats aToD = SSubAll(d, a);
    SFloats aToO = SVec3::neg(a);
    SFloats acdNorm = SVec3::cross(aToC, aToD);
    SFloats acdRegion = SVec3::dot(acdNorm, aToO);

    SFloats combined = sCombine(bdcRegion, acdRegion, adbRegion);
    SAlign float bdc_acd_adb[4];
    SStoreAll(bdc_acd_adb, combined);

    //Figure out which regions it's in front of, going through all the combinations
    bool bdc = bdc_acd_adb[0] > 0.0f;
    bool acd = bdc_acd_adb[1] > 0.0f;
    bool adb = bdc_acd_adb[2] > 0.0f;
    SFloats result;
    //In front of all faces
    if(bdc && acd && adb)
      result = _sThreeTriCase(bdcNorm, acdNorm, badNorm);
    else if(bdc && acd)
      result = _sTwoTriCase(SupportID::D, SupportID::C, SupportID::B, SupportID::A, bdcNorm, acdNorm);
    else if(bdc && adb)
      result = _sTwoTriCase(SupportID::D, SupportID::B, SupportID::A, SupportID::C, badNorm, bdcNorm);
    else if(acd && adb)
      result = _sTwoTriCase(SupportID::D, SupportID::A, SupportID::C, SupportID::B, acdNorm, badNorm);
    else if(bdc)
      result = _sOneTriCase(SupportID::D, SupportID::C, SupportID::B, SupportID::A, bdcNorm);
    else if(acd)
      result = _sOneTriCase(SupportID::D, SupportID::A, SupportID::C, SupportID::B, acdNorm);
    else if(adb)
      result = _sOneTriCase(SupportID::D, SupportID::B, SupportID::A, SupportID::C, badNorm);
    else {
      //Not in front of any face. Collision.
      mContainsOrigin = true;
      mCheckDirection = false;
      result = SVec3::Zero;
    }

    return result;
  }

  SFloats Simplex::_sOneTriCase(int a, int b, int c, int d, const SFloats& norm) {
    //For this case the origin is in front of only the abc triangle, so solve for that
    SFloats pointA = SLoadAll(&get(a).x);
    SFloats pointB = SLoadAll(&get(b).x);
    SFloats pointC = SLoadAll(&get(c).x);
    int region;
    //Could go back and reorder region tests so these don't need to be computed, just re-used
    SFloats result = _sSolveTriangle(SSubAll(pointC, pointA), SSubAll(pointB, pointA), SVec3::neg(pointA), norm, pointA, pointB, pointC, region, false);
    _sDiscardTetrahedron(a, b, c, d, region);
    return result;
  }

  SFloats Simplex::_sTwoTriCase(int a, int b, int c, int d, const SFloats& abcNorm, const SFloats& adbNorm) {
    //For two face cases it should either be within one of them or on the line between them,
    //in which case it doesn't matter which closest we pick
    SFloats pointA = SLoadAll(&get(a).x);
    SFloats pointB = SLoadAll(&get(b).x);
    SFloats pointC = SLoadAll(&get(c).x);
    SFloats pointD = SLoadAll(&get(d).x);

    int abcReg, adbReg;
    SFloats abcResult, adbResult, result;
    abcResult = _sSolveTriangle(SSubAll(pointC, pointA), SSubAll(pointB, pointA), SVec3::neg(pointA), abcNorm, pointA, pointB, pointC, abcReg, false);
    adbResult = _sSolveTriangle(SSubAll(pointB, pointA), SSubAll(pointD, pointA), SVec3::neg(pointA), adbNorm, pointA, pointD, pointB, adbReg, false);
    if(abcReg == SRegion::Face) {
      result = abcResult;
      _sDiscardTetrahedron(a, b, c, d, abcReg);
    }
    else {
      result = adbResult;
      _sDiscardTetrahedron(a, d, b, c, adbReg);
    }

    return result;
  }

  SFloats Simplex::_sThreeTriCase(const SFloats& bdcNorm, const SFloats& acdNorm, const SFloats& badNorm) {
    //In this case, assume that if it's in front of a face, the closest point on that face is best,
    //and if it isn't in front of any face then it must be in the vertex region,
    //in which case all three triangle results are the same
    SFloats a = SLoadAll(&get(SupportID::A).x);
    SFloats b = SLoadAll(&get(SupportID::B).x);
    SFloats c = SLoadAll(&get(SupportID::C).x);
    SFloats d = SLoadAll(&get(SupportID::D).x);

    int bdcReg, acdReg, adbReg;
    SFloats bdcResult, acdResult, adbResult, result;
    //Should go back and reorder region tests so these don't need to be computed, just re-used
    bdcResult = _sSolveTriangle(SSubAll(b, d), SSubAll(c, d), SVec3::neg(d), bdcNorm, d, c, b, bdcReg, false);
    acdResult = _sSolveTriangle(SSubAll(c, d), SSubAll(a, d), SVec3::neg(d), acdNorm, d, a, c, acdReg, false);
    adbResult = _sSolveTriangle(SSubAll(a, d), SSubAll(b, d), SVec3::neg(d), badNorm, d, b, a, adbReg, false);
    if(bdcReg == SRegion::Face) {
      result = bdcResult;
      _sDiscardTetrahedron(SupportID::D, SupportID::C, SupportID::B, SupportID::A, bdcReg);
    }
    else if(acdReg == SRegion::Face) {
      result = acdResult;
      _sDiscardTetrahedron(SupportID::D, SupportID::A, SupportID::C, SupportID::B, acdReg);
    }
    else {
      result = adbResult;
      _sDiscardTetrahedron(SupportID::D, SupportID::B, SupportID::A, SupportID::C, adbReg);
    }

    return result;
  }

  void Simplex::_sDiscardTetrahedron(int, int b, int c, int d, int region) {
    //A is never thrown away in the tetrahedron case
    switch(region) {
      case SRegion::A: _discard(b, c, d); break;
      case SRegion::AB: _discard(c, d); break;
      case SRegion::AC: _discard(b, d); break;
      case SRegion::Face:
        _discard(d);
        _fixWinding();
        break;
      case SRegion::FaceAwayO: break;
      default: SyxAssertError(false, "Unhandled region passed to DiscardTetrahedron");
    }
  }

  bool Simplex::sIsDegenerate(void) {
    switch(mSize) {
      case 0:
      case 1:
        return false;
      case 2:
        return get(SupportID::A) == get(SupportID::B);
      case 3:
      {
        //Degenerate if triangle normal is zero
        SFloats a = SLoadAll(&get(SupportID::A).x);
        SFloats b = SLoadAll(&get(SupportID::B).x);
        SFloats c = SLoadAll(&get(SupportID::C).x);
        SFloats n = SVec3::cross(SSubAll(b, a), SSubAll(c, a));
        SFloats nLen = SVec3::dot(n, n);
        return SILessLower(nLen, SVec3::Epsilon) != 0;
      }
      case 4:
        //Get abc normal and find distance along normal to d. If this is zero then this is degenerate, either because the normal is zero or distance is zero
        SFloats a = SLoadAll(&get(SupportID::A).x);
        SFloats b = SLoadAll(&get(SupportID::B).x);
        SFloats c = SLoadAll(&get(SupportID::C).x);
        SFloats d = SLoadAll(&get(SupportID::D).x);
        SFloats n = SVec3::cross(SSubAll(b, a), SSubAll(c, a));
        SFloats dDist = sAbsAll(SVec3::dot(n, d));
        return SILessLower(dDist, SVec3::Epsilon) != 0;
    }

    //Doesn't matter, shouldn't happen
    return true;
  }

  bool Simplex::sMakesProgress(SFloats newPoint, SFloats searchDir) {
    if(!mSize)
      return true;
    SFloats newDot = SVec3::dot(searchDir, newPoint);
    SFloats bestDot = SVec3::dot(searchDir, SLoadAll(&get(SupportID::A).x));
    //Store the biggest  dot product in bestDot[0]
    for(size_t i = 1; i < mSize; ++i)
      bestDot = SMaxLower(bestDot, SVec3::dot(searchDir, SLoadAll(&get(i).x)));

    //If new value is greatest dot product, this is progress
    return SIGreaterLower(newDot, bestDot) != 0;
  }

#else
  SFloats Simplex::sSolve(void) { return SVec3::Zero; }
  SFloats Simplex::_sSolveLine(void) { return SVec3::Zero; }
  SFloats Simplex::_sSolveTriangle(void) { return SVec3::Zero; }
  SFloats Simplex::_sSolveTriangle(const SFloats&, const SFloats&, const SFloats&, const SFloats&, const SFloats&, const SFloats&, const SFloats&, int&, bool) { return SVec3::Zero; }
  SFloats Simplex::_sSolveTetrahedron(void) { return SVec3::Zero; }
  void Simplex::_sDiscardTetrahedron(int, int, int, int, int) {}
  SFloats Simplex::_sOneTriCase(int, int, int, int, const SFloats&) { return SVec3::Zero; }
  SFloats Simplex::_sTwoTriCase(int, int, int, int, const SFloats&, const SFloats&) { return SVec3::Zero; }
  SFloats Simplex::_sThreeTriCase(const SFloats&, const SFloats&, const SFloats&) { return SVec3::Zero; }
  bool Simplex::sIsDegenerate(void) { return false; }
#endif
}