#pragma once
#include "SyxSimplex.h"

namespace Syx
{
  class DebugSimplex
  {
  public:
    DebugSimplex(void) { Initialize(); }
    DebugSimplex(const Simplex& rhs) { *this = rhs; }

    void Initialize(void) { m_size = 0; m_history.clear(); }
    //Returns if added point was a duplicate and checkForDuplicates is true
    bool Add(const SupportPoint& toAdd, bool checkForDuplicates);
    //Evaluate the simplex, throwing out points not helping contain the origin and return new search direction
    Vec3 Solve(void);
    void GrowToFourPoints(Narrowphase& narrow);

    bool Contains(const Vec3& support) const;
    bool ContainsOrigin(void);
    bool IsDegenerate(void) const;
    bool MakesProgress(const Vec3& newPoint, const Vec3& searchDir) const;

    const Vec3& Get(int id) const { return m_supports[id].mSupport; }
    SupportPoint& GetSupport(int id) { return m_supports[id]; }
    size_t Size(void) { return m_size; }

    bool operator==(const Simplex& rhs);
    bool operator!=(const Simplex& rhs);
    DebugSimplex& operator=(const Simplex& rhs);

  private:
    Vec3 SolveLine(void);
    Vec3 SolveTriangle(void);
    Vec3 SolveTetrahedron(void);

    Vec3 DebugSimplex::GetTri(int index, Vec3& ra, Vec3& rb, Vec3& rc) const;

    void Discard(const Vec3& point);
    void Discard(int id);
    void Discard(int a, int b);
    void Discard(int a, int b, int c);
    void FixWinding(void);

    void PrintState(void);
    std::string GetIndexName(int index);

    SAlign SupportPoint m_supports[SIMPLEX_MAX];
    size_t m_size;
    std::string m_history;
  };
}