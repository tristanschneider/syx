#pragma once
#include "SyxSimplex.h"

namespace Syx {
  class DebugSimplex {
  public:
    DebugSimplex() { initialize(); }
    DebugSimplex(const Simplex& rhs) { *this = rhs; }

    void initialize() { mSize = 0; mHistory.clear(); }
    //Returns if added point was a duplicate and checkForDuplicates is true
    bool add(const SupportPoint& toAdd, bool checkForDuplicates);
    //Evaluate the simplex, throwing out points not helping contain the origin and return new search direction
    Vec3 solve();
    void growToFourPoints(Narrowphase& narrow);

    bool contains(const Vec3& support) const;
    bool containsOrigin();
    bool isDegenerate() const;
    bool makesProgress(const Vec3& newPoint, const Vec3& searchDir) const;

    const Vec3& get(int id) const { return mSupports[id].mSupport; }
    SupportPoint& getSupport(int id) { return mSupports[id]; }
    size_t size() { return mSize; }

    bool operator==(const Simplex& rhs);
    bool operator!=(const Simplex& rhs);
    DebugSimplex& operator=(const Simplex& rhs);

  private:
    Vec3 _solveLine();
    Vec3 _solveTriangle();
    Vec3 _solveTetrahedron();

    Vec3 _getTri(int index, Vec3& ra, Vec3& rb, Vec3& rc) const;

    void _discard(const Vec3& point);
    void _discard(int id);
    void _discard(int a, int b);
    void _discard(int a, int b, int c);
    void _fixWinding();

    void _printState();
    std::string _getIndexName(int index);

    SAlign SupportPoint mSupports[SIMPLEX_MAX];
    size_t mSize;
    std::string mHistory;
  };
}