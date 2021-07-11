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

    const Vec3& get(size_t id) const { return mSupports[id].mSupport; }
    const Vec3& get(SupportID id) const { return mSupports[size_t(id)].mSupport; }
    SupportPoint& getSupport(size_t id) { return mSupports[id]; }
    SupportPoint& getSupport(SupportID id) { return mSupports[size_t(id)]; }
    const SupportPoint& getSupport(SupportID id) const { return mSupports[size_t(id)]; }
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
    void _discard(size_t id);
    void _discard(size_t a, size_t b);
    void _discard(size_t a, size_t b, size_t c);
    void _discard(SupportID id) { _discard(size_t(id)); };
    void _discard(SupportID a, SupportID b) { _discard(size_t(a), size_t(b)); }
    void _discard(SupportID a, SupportID b, SupportID c) { _discard(size_t(a), size_t(b), size_t(c)); }
    void _fixWinding();

    void _printState();
    std::string _getIndexName(size_t index);

    SAlign SupportPoint mSupports[SIMPLEX_MAX];
    size_t mSize;
    std::string mHistory;
  };
}