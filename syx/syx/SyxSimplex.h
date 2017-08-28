#pragma once

#define SIMPLEX_MAX 4

namespace Syx {
  class Narrowphase;

  SAlign struct SupportPoint {
    SupportPoint(void) {}
    SupportPoint(const Vec3& support): mSupport(support) {}
    SupportPoint(const Vec3& a, const Vec3& b): mPointA(a), mPointB(b), mSupport(a - b) {}
    SupportPoint(const Vec3& a, const Vec3& b, const Vec3& amb): mPointA(a), mPointB(b), mSupport(amb) {}

    bool operator==(const SupportPoint& rhs) { return mSupport == rhs.mSupport; }
    bool operator!=(const SupportPoint& rhs) { return mSupport != rhs.mSupport; }

    //Support points from object a and b respectively
    SAlign Vec3 mPointA;
    SAlign Vec3 mPointB;
    //pointA - pointB
    SAlign Vec3 mSupport;
  };

  namespace SupportID {
    //In increasing order of age, so A was added first, followed by B, etc.
    enum {
      A,
      B,
      C,
      D,
      None
    };
  }

  namespace SRegion {
    enum {
      A,
      AB,
      AC,
      //Within triangle, didn't check normal
      Face,
      //Within triangle whos normal is towards origin
      FaceToO,
      //Within triangle whos normal is away from origin
      FaceAwayO
    };
  }

  SAlign class Simplex {
  public:
    Simplex(void) { initialize(); }

    void initialize(void) { mSize = 0; mContainsOrigin = mDegenerate = false; }
    //Returns if added point was a duplicate and checkForDuplicates is true
    bool add(const SupportPoint& toAdd, bool checkForDuplicates);
    //Evaluate the simplex, throwing out points not helping contain the origin and return new search direction
    Vec3 solve(void);
    void growToFourPoints(Narrowphase& narrow);

    bool contains(const Vec3& support) const;
    bool containsOrigin(void) const { return mContainsOrigin; }
    bool isDegenerate(void) const { return mDegenerate; }
    bool makesProgress(const Vec3& newPoint, const Vec3& searchDir) const;

    void draw(const Vec3& searchDir);

    //Used like Get(SupportID::B)
    const Vec3& get(int id) const { return mSupports[id].mSupport; }
    SupportPoint& getSupport(int id) { return mSupports[id]; }
    const SupportPoint& getSupport(int id) const { return mSupports[id]; }
    size_t size(void) const { return mSize; }

    bool sIsDegenerate(void);
    SFloats sSolve(void);
    bool sMakesProgress(SFloats newPoint, SFloats searchDir);

  private:
    void _discard(int id);
    void _discard(int a, int b);
    void _discard(int a, int b, int c);
    void _fixWinding(void);

    Vec3 _solveLine(void);
    Vec3 _solveTriangle(void);
    Vec3 _solveTetrahedron(void);

    SFloats _sSolveLine(void);
    SFloats _sSolveTriangle(void);
    //The letters don't refer to the SupportID but rather with a being the only vertex the origin could be in front of
    SFloats _sSolveTriangle(const SFloats& aToC, const SFloats& aToB, const SFloats& aToO, const SFloats& normal, const SFloats& a, const SFloats& b, const SFloats& c, int& resultRegion, bool checkWinding);
    SFloats _sSolveTetrahedron(void);
    void _sDiscardTetrahedron(int a, int b, int c, int d, int region);
    //Cases within tetrahedron, origin visible by one or two triangles
    //a is the vertex the origin could be in front of, and d is the vertex that isn't part of the triangle
    SFloats _sOneTriCase(int a, int b, int c, int d, const SFloats& norm);
    //first triangle is abc, second is adb
    SFloats _sTwoTriCase(int a, int b, int c, int d, const SFloats& abcNorm, const SFloats& adbNorm);
    SFloats _sThreeTriCase(const SFloats& bdcNorm, const SFloats& acdNorm, const SFloats& badNorm);

    SAlign SupportPoint mSupports[SIMPLEX_MAX];
    size_t mSize;
    bool mContainsOrigin;
    bool mDegenerate;
    //Used in simd solving to avoid having to check in control paths that can do it without an extra unload
    bool mCheckDirection;
  };
}