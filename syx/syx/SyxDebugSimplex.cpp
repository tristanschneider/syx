#include "Precompile.h"
#include "SyxDebugSimplex.h"
#include "SyxNarrowphase.h"

namespace Syx {
  bool DebugSimplex::add(const SupportPoint& toAdd, bool checkForDuplicates) {
    if(checkForDuplicates)
      for(size_t i = 0; i < mSize; ++i)
        if(mSupports[i].mSupport == toAdd.mSupport)
          return false;

    mSupports[mSize++] = toAdd;
    return true;
  }

  Vec3 DebugSimplex::solve(void) {
    switch(mSize) {
    case 1: mHistory += "Solve point\n"; return -mSupports[0].mSupport;
    case 2: return _solveLine();
    case 3: return _solveTriangle();
    case 4: return _solveTetrahedron();
    default: SyxAssertError(false, "Nonsense size in debug solve"); return Vec3::Zero;
    }
  }

  void DebugSimplex::growToFourPoints(Narrowphase& narrow) {
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
      Quat rot = Quat::axisAngle(lineSeg, 3.14f / 3.0f);
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

  bool DebugSimplex::contains(const Vec3& support) const {
    for(size_t i = 0; i < mSize; ++i)
      if(mSupports[i].mSupport == support)
        return true;
    return false;
  }

  Vec3 DebugSimplex::_getTri(int index, Vec3& ra, Vec3& rb, Vec3& rc) const {
    // B.
    // | C
    // |/   D
    // A
    const Vec3& a = mSupports[0].mSupport;
    const Vec3& b = mSupports[1].mSupport;
    const Vec3& c = mSupports[2].mSupport;
    const Vec3& d = mSupports[3].mSupport;
    // Arbitrary ordering
    switch(index) {
    case 0:
      ra = a;
      rb = d;
      rc = b;
      return c;
    case 1:
      ra = d;
      rb = c;
      rc = b;
      return a;
    case 2:
      ra = c;
      rb = a;
      rc = b;
      return d;
    case 3:
      ra = d;
      rb = a;
      rc = c;
      return b;
    default: SyxAssertError(false, "Nonsense index"); return Vec3::Zero;
    }
  }

  bool DebugSimplex::containsOrigin(void) {
    if(isDegenerate())
      SyxAssertError(false, "Don't want to deal with figuring out if degenerate shape contains origin");

    Vec3 o = Vec3::Zero;
    const Vec3& a = get(SupportID::A);
    const Vec3& b = get(SupportID::B);
    const Vec3& c = get(SupportID::C);

    switch(mSize) {
    case 1: return a == o;
    case 2:
    {
      Vec3 ao = o - a;
      Vec3 ab = b - a;
      float t = Vec3::projVecScalar(ao, ab);
      if(t < 0.0f || t > 1.0f)
        return false;
      return o == a + ab*t;
    }
    case 3: return closestOnTri(o, a, b, c) == o;
    case 4:
    {
      _fixWinding();
      Vec3 ta, tb, tc;
      for(int i = 0; i < 4; ++i) {
        _getTri(i, ta, tb, tc);
        if(triangleNormal(ta, tb, tc).dot(o - ta) > 0.0f)
          return false;
      }
      return true;
    }
    default: SyxAssertError(false, "Nonsense size"); return false;
    }
  }

  bool DebugSimplex::isDegenerate(void) const {
    const Vec3& a = get(SupportID::A);
    const Vec3& b = get(SupportID::B);
    const Vec3& c = get(SupportID::C);
    const Vec3& d = get(SupportID::D);

    switch(mSize) {
    case 1: return false;
    case 2: return a == b;
    case 3: return triangleNormal(a, b, c).length() < SYX_EPSILON;
    case 4: return std::abs((d - a).dot(triangleNormal(a, b, c))) < SYX_EPSILON;
    default: SyxAssertError(false, "Nonsense size"); return false;
    }
  }

  bool DebugSimplex::makesProgress(const Vec3& newPoint, const Vec3& searchDir) const {
    float newDot = newPoint.dot(searchDir);
    for(size_t i = 0; i < mSize; ++i)
      if(mSupports[i].mSupport.dot(searchDir) >= newDot)
        return false;
    return true;
  }

  Vec3 DebugSimplex::_solveLine(void) {
    mHistory += "Solve line\n";
    _printState();

    Vec3 a = get(SupportID::A);
    Vec3 b = get(SupportID::B);

    Vec3 aToB = b - a;
    float proj = Vec3::projVecScalar(-a, aToB);
    if(proj >= 1.0f) {
      proj = 1.0f;
      _discard(SupportID::A);
    }
    else if(proj <= 0.0f) {
      proj = 0.0f;
      _discard(SupportID::B);
    }
    return -(a + aToB*proj);
  }

  Vec3 DebugSimplex::_solveTriangle(void) {
    mHistory += "Solve triangle\n";
    _printState();

    Vec3 a = get(SupportID::A);
    Vec3 b = get(SupportID::B);
    Vec3 c = get(SupportID::C);
    //Vec3 n = TriangleNormal(a, b, c);
    //
    //Vec3 abNorm = (b - a).Cross(n).SafeNormalized();
    //Vec3 bcNorm = (c - b).Cross(n).SafeNormalized();
    //Vec3 caNorm = (a - c).Cross(n).SafeNormalized();
    //bool lineCase = true;
    //Vec3 lineResult;
    //float abDot = abNorm.dot(-a);
    //float bcDot = bcNorm.dot(-b);
    //float caDot = caNorm.dot(-a);
    //
    //if(abDot >= 0.0f)
    //{
    //  Discard(SupportID::C);
    //  lineResult = SolveLine();
    //}
    //else if(bcDot >= 0.0f)
    //{
    //  Discard(SupportID::A);
    //  lineResult = SolveLine();
    //}
    //else if(caDot >= 0.0f)
    //{
    //  Discard(SupportID::B);
    //  lineResult = SolveLine();
    //}
    //else
    //  lineCase = false;

    // s=aToB
    // t=aToC
    float s, t;
    float one = 1.0f;
    bool lineCase = true;
    bool wasClamped;
    Vec3 result = -closestOnTri(Vec3::Zero, a, b, c, &s, &t, &wasClamped);
    if(s == 0.0f && t == 0.0f)
      _discard(SupportID::B, SupportID::C);
    else if(s == 0.0f) {
      if(t >= one)
        _discard(SupportID::A, SupportID::B);
      else
        _discard(SupportID::B);
    }
    else if(t == 0.0f) {
      if(s >= one)
        _discard(SupportID::A, SupportID::C);
      else
        _discard(SupportID::C);
    }
    else if(s + t >= one)
      _discard(SupportID::A);
    else
      lineCase = false;

    if(!lineCase)
      _fixWinding();

    if(lineCase) {
      Vec3 test = solve();
      SyxAssertError(test.equal(result, 0.01f), "Wrong result");
    }
    //SyxAssertError(!lineCase || result == SolveLine(), "Made a mistake with barycentric discarding");

    //Vec3 result = -ClosestOnTri(Vec3::Zero, a, b, c, &s, &t);
    //if(lineCase && lineResult != result)
    //  __debugbreak();
    //SyxAssertError(!lineCase || lineResult == result, "Results don't match, indicating bogus");
    return result;
  }

  struct Test {
    Test(float inS, float inT, const Vec3& inP, bool b) : s(inS), t(inT), p(inP), clamped(b) {}
    Test(void) {}

    float s, t;
    Vec3 p, o;
    bool clamped;
    float oproj;
  };

  Vec3 DebugSimplex::_solveTetrahedron(void) {
    mHistory += "Solve tetrahedron\n";
    _printState();

    _fixWinding();
    float dots[4];
    Test test[4];
    Vec3 bestPoint;
    int bestIndex = -1;
    for(int i = 0; i < 4; ++i) {
      Vec3 a, b, c;
      Vec3 o = _getTri(i, a, b, c);
      dots[i] = triangleNormal(a, b, c).dot(-a);
      float s, t;
      bool clamped;
      Vec3 closest = closestOnTri(Vec3::Zero, a, b, c, &s, &t, &clamped);
      if(dots[i] && (bestIndex == -1 || closest.length2() < bestPoint.length2())) {
        bestIndex = i;
        bestPoint = closest;
      }

      Test f;
      f.p = closest;
      f.o = o;
      f.s = s;
      f.t = t;
      f.clamped = clamped;
      f.oproj = dots[i];

      test[i] = f;
    }

    bool inFront = false;
    for(int i = 0; i < 4; ++i) {
      Vec3 a, b, c;
      Vec3 other = _getTri(i, a, b, c);
      if(triangleNormal(a, b, c).dot(-a) > 0.0f) {
        inFront = true;
        bool wasClamped;
        closestOnTri(Vec3::Zero, a, b, c, nullptr, nullptr, &wasClamped);
        if(wasClamped)
          continue;

        _discard(other);
        Vec3 result = _solveTriangle();
        if(!result.equal(-bestPoint, 0.01f))
          __debugbreak();
        SyxAssertError(result.equal(-bestPoint, 0.01f), "Wrong point");
        return result;
      }
    }
    // In front of one or more faces, but not contained by any
    if(inFront) {
      Vec3 a, b, c;
      _discard(_getTri(bestIndex, a, b, c));
      Vec3 result = _solveTriangle();
      SyxAssertError(result.equal(-bestPoint, 0.01f), "Wrong point");
      return result;
    }

    return Vec3::Zero;
  }

  void DebugSimplex::_discard(const Vec3& point) {
    for(size_t i = 0; i < mSize; ++i)
      if(point == get(i)) {
        _discard(i);
        return;
      }
    SyxAssertError(false, "Tried to discard point that wasn't in simplex");
  }

  void DebugSimplex::_discard(size_t id) {
    mHistory += "Discard " + _getIndexName(id);
    mHistory += "\n";

    --mSize;
    for(size_t i = static_cast<size_t>(id); i < mSize; ++i)
      mSupports[i] = mSupports[i + 1];
  }

  void DebugSimplex::_discard(size_t a, size_t b) {
    if(a > b)
      std::swap(a, b);
    _discard(b);
    _discard(a);
  }

  void DebugSimplex::_discard(size_t a, size_t b, size_t c) {
    orderAscending(a, b, c);
    _discard(c);
    _discard(b);
    _discard(a);
  }

  void DebugSimplex::_fixWinding(void) {
    Vec3 normal = triangleNormal(get(SupportID::A), get(SupportID::B), get(SupportID::C));
    float dot = normal.dot(-get(SupportID::A));
    if(dot > 0.0f) {
      std::swap(getSupport(SupportID::B), getSupport(SupportID::C));
      mHistory += "Fixed winding\n";
    }
  }

  void DebugSimplex::_printState(void) {
    mHistory += "Simplex Points:\n";

    for(size_t i = 0; i < mSize; ++i) {
      const Vec3& p = get(i);
      mHistory += _getIndexName(i) + ": (" + std::to_string(p.x) + ", " + std::to_string(p.y) + ", " + std::to_string(p.z) + ")\n";
    }
  }

  std::string DebugSimplex::_getIndexName(size_t index) {
    switch(index) {
    case 0: return "A";
    case 1: return "B";
    case 2: return "C";
    case 3: return "D";
      //I for invalid
    default: return "I";
    }
  }

  bool DebugSimplex::operator==(const Simplex& rhs) {
    if(mSize != rhs.size())
      return false;

    for(size_t i = 0; i < mSize; ++i)
      if(get(i) != rhs.get(i))
        return false;
    return true;
  }

  bool DebugSimplex::operator!=(const Simplex& rhs) {
    return !(*this == rhs);
  }

  DebugSimplex& DebugSimplex::operator=(const Simplex& rhs) {
    mSize = rhs.size();
    for(size_t i = 0; i < mSize; ++i)
      mSupports[i] = rhs.getSupport(i);
    return *this;
  }
}