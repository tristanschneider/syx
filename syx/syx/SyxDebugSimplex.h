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
    Vector3 Solve(void);
    void GrowToFourPoints(Narrowphase& narrow);

    bool Contains(const Vector3& support) const;
    bool ContainsOrigin(void);
    bool IsDegenerate(void) const;
    bool MakesProgress(const Vector3& newPoint, const Vector3& searchDir) const;

    const Vector3& Get(int id) const { return m_supports[id].mSupport; }
    SupportPoint& GetSupport(int id) { return m_supports[id]; }
    size_t Size(void) { return m_size; }

    bool operator==(const Simplex& rhs);
    bool operator!=(const Simplex& rhs);
    DebugSimplex& operator=(const Simplex& rhs);

  private:
    Vector3 SolveLine(void);
    Vector3 SolveTriangle(void);
    Vector3 SolveTetrahedron(void);

    Vector3 DebugSimplex::GetTri(int index, Vector3& ra, Vector3& rb, Vector3& rc) const;

    void Discard(const Vector3& point);
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