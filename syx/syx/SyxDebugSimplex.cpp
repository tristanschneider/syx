#include "Precompile.h"
#include "SyxDebugSimplex.h"
#include "SyxNarrowphase.h"

namespace Syx
{
  bool DebugSimplex::Add(const SupportPoint& toAdd, bool checkForDuplicates)
  {
    if(checkForDuplicates)
      for(size_t i = 0; i < m_size; ++i)
        if(m_supports[i].mSupport == toAdd.mSupport)
          return false;

    m_supports[m_size++] = toAdd;
    return true;
  }

  Vector3 DebugSimplex::Solve(void)
  {
    switch(m_size)
    {
    case 1: m_history += "Solve point\n"; return -m_supports[0].mSupport;
    case 2: return SolveLine();
    case 3: return SolveTriangle();
    case 4: return SolveTetrahedron();
    default: SyxAssertError(false, "Nonsense size in debug solve"); return Vector3::Zero;
    }
  }

  void DebugSimplex::GrowToFourPoints(Narrowphase& narrow)
  {
    //Possible directions to look for support points
    static SAlign Vector3 searchDirs[] =
    {
      Vector3::UnitX, Vector3::UnitY, Vector3::UnitZ,
      -Vector3::UnitX, -Vector3::UnitY, -Vector3::UnitZ,
    };

    //Intentional fall through cases
    switch(m_size)
    {
    //Already has four, get out of here
    case 4:
      return;

    case 0:
      //Arbitrary direction. Doesn't matter since there are no others for it to be a duplicate of
      Add(narrow.GetSupport(Vector3::UnitY), false);

    case 1:
      for(Vector3& curDir : searchDirs)
      {
        SupportPoint curPoint = narrow.GetSupport(curDir);
        if(curPoint.mSupport.Distance2(m_supports[0].mSupport) > SYX_EPSILON)
        {
          Add(curPoint, false);
          break;
        }
      }

    case 2:
    {
      //Get closest point to origin on line segment
      Vector3 lineSeg = (m_supports[1].mSupport - m_supports[0].mSupport).SafeNormalized();
      int leastSignificantAxis = lineSeg.LeastSignificantAxis();
      SAlign Vector3 searchDir = lineSeg.Cross(searchDirs[leastSignificantAxis]);
      //Matrix would be a bit faster, but I don't imagine this case comes
      //up often enough for it to matter
      Quat rot = Quat::AxisAngle(lineSeg, 3.14f/3.0f);
      SupportPoint newPoint = narrow.GetSupport(searchDir);

      for(unsigned i = 0; i < 6; ++i)
      {
        SupportPoint curPoint = narrow.GetSupport(searchDir);
        if(Vector3::PointLineDistanceSQ(curPoint.mSupport, m_supports[0].mSupport, m_supports[1].mSupport) > SYX_EPSILON)
        {
          newPoint = curPoint;
          break;
        }

        searchDir = rot * searchDir;
      }
      Add(newPoint, false);
    }
    case 3:
    {
      SAlign Vector3 searchDir = TriangleNormal(m_supports[2].mSupport,
        m_supports[1].mSupport, m_supports[0].mSupport);

      SupportPoint newPoint = narrow.GetSupport(searchDir);
      //If this point matches one of the other points already, search in a different direction
      for(unsigned i = 0; i < 3; ++i)
        if(m_supports[i].mSupport == newPoint.mSupport)
        {
          //For flat shapes this could still result in a duplicate, but we can't do anything better from here
          SAlign Vector3 negDir = -searchDir;
          newPoint = narrow.GetSupport(negDir);
          break;
        }

      Add(newPoint, false);
    }
    }

    //Fix winding
    Vector3 v30 = m_supports[0].mSupport - m_supports[3].mSupport;
    Vector3 v31 = m_supports[1].mSupport - m_supports[3].mSupport;
    Vector3 v32 = m_supports[2].mSupport - m_supports[3].mSupport;
    float det = Vector3::Dot(v30, Vector3::Cross(v31, v32));
    if(det <= 0.0f)
      std::swap(m_supports[0], m_supports[1]);
  }

  bool DebugSimplex::Contains(const Vector3& support) const
  {
    for(size_t i = 0; i < m_size; ++i)
      if(m_supports[i].mSupport == support)
        return true;
    return false;
  }

  Vector3 DebugSimplex::GetTri(int index, Vector3& ra, Vector3& rb, Vector3& rc) const
  {
    // B.
    // | C
    // |/   D
    // A
    const Vector3& a = m_supports[0].mSupport;
    const Vector3& b = m_supports[1].mSupport;
    const Vector3& c = m_supports[2].mSupport;
    const Vector3& d = m_supports[3].mSupport;
    // Arbitrary ordering
    switch(index)
    {
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
    default: SyxAssertError(false, "Nonsense index"); return Vector3::Zero;
    }
  }

  bool DebugSimplex::ContainsOrigin(void)
  {
    if(IsDegenerate())
      SyxAssertError(false, "Don't want to deal with figuring out if degenerate shape contains origin");

    Vector3 o = Vector3::Zero;
    const Vector3& a = Get(SupportID::A);
    const Vector3& b = Get(SupportID::B);
    const Vector3& c = Get(SupportID::C);

    switch(m_size)
    {
      case 1: return a == o;
      case 2:
      {
        Vector3 ao = o - a;
        Vector3 ab = b - a;
        float t = Vector3::ProjVecScalar(ao, ab);
        if(t < 0.0f || t > 1.0f)
          return false;
        return o == a + ab*t;
      }
      case 3: return ClosestOnTri(o, a, b, c) == o;
      case 4:
      {
        FixWinding();
        Vector3 ta, tb, tc;
        for(int i = 0; i < 4; ++i)
        {
          GetTri(i, ta, tb, tc);
          if(TriangleNormal(ta, tb, tc).Dot(o - ta) > 0.0f)
            return false;
        }
        return true;
      }
      default: SyxAssertError(false, "Nonsense size"); return false;
    }
  }

  bool DebugSimplex::IsDegenerate(void) const
  {
    const Vector3& a = Get(SupportID::A);
    const Vector3& b = Get(SupportID::B);
    const Vector3& c = Get(SupportID::C);
    const Vector3& d = Get(SupportID::D);

    switch(m_size)
    {
    case 1: return false;
    case 2: return a == b;
    case 3: return TriangleNormal(a, b, c).Length() < SYX_EPSILON;
    case 4: return std::abs((d - a).Dot(TriangleNormal(a, b, c))) < SYX_EPSILON;
    default: SyxAssertError(false, "Nonsense size"); return false;
    }
  }

  bool DebugSimplex::MakesProgress(const Vector3& newPoint, const Vector3& searchDir) const
  {
    float newDot = newPoint.Dot(searchDir);
    for(size_t i = 0; i < m_size; ++i)
      if(m_supports[i].mSupport.Dot(searchDir) >= newDot)
        return false;
    return true;
  }

  Vector3 DebugSimplex::SolveLine(void)
  {
    m_history += "Solve line\n";
    PrintState();

    Vector3 a = Get(SupportID::A);
    Vector3 b = Get(SupportID::B);

    Vector3 aToB = b - a;
    float proj = Vector3::ProjVecScalar(-a, aToB);
    if(proj >= 1.0f)
    {
      proj = 1.0f;
      Discard(SupportID::A);
    }
    else if(proj <= 0.0f)
    {
      proj = 0.0f;
      Discard(SupportID::B);
    }
    return -(a + aToB*proj);
  }

  Vector3 DebugSimplex::SolveTriangle(void)
  {
    m_history += "Solve triangle\n";
    PrintState();

    Vector3 a = Get(SupportID::A);
    Vector3 b = Get(SupportID::B);
    Vector3 c = Get(SupportID::C);
    //Vector3 n = TriangleNormal(a, b, c);
    //
    //Vector3 abNorm = (b - a).Cross(n).SafeNormalized();
    //Vector3 bcNorm = (c - b).Cross(n).SafeNormalized();
    //Vector3 caNorm = (a - c).Cross(n).SafeNormalized();
    //bool lineCase = true;
    //Vector3 lineResult;
    //float abDot = abNorm.Dot(-a);
    //float bcDot = bcNorm.Dot(-b);
    //float caDot = caNorm.Dot(-a);
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
    Vector3 result = -ClosestOnTri(Vector3::Zero, a, b, c, &s, &t, &wasClamped);
    if(s == 0.0f && t == 0.0f)
      Discard(SupportID::B, SupportID::C);
    else if(s == 0.0f)
    {
      if(t >= one)
        Discard(SupportID::A, SupportID::B);
      else
        Discard(SupportID::B);
    }
    else if(t == 0.0f)
    {
      if(s >= one)
        Discard(SupportID::A, SupportID::C);
      else
        Discard(SupportID::C);
    }
    else if(s + t >= one)
      Discard(SupportID::A);
    else
      lineCase = false;

    if(!lineCase)
      FixWinding();

    if(lineCase)
    {
      Vector3 test = Solve();
      SyxAssertError(test.Equal(result, 0.01f), "Wrong result");
    }
    //SyxAssertError(!lineCase || result == SolveLine(), "Made a mistake with barycentric discarding");

    //Vector3 result = -ClosestOnTri(Vector3::Zero, a, b, c, &s, &t);
    //if(lineCase && lineResult != result)
    //  __debugbreak();
    //SyxAssertError(!lineCase || lineResult == result, "Results don't match, indicating bogus");
    return result;
  }

  struct Test
  {
    Test(float inS, float inT, const Vector3& inP, bool b): s(inS), t(inT), p(inP), clamped(b) {}
    Test(void) {}

    float s, t;
    Vector3 p, o;
    bool clamped;
    float oproj;
  };

  Vector3 DebugSimplex::SolveTetrahedron(void)
  {
    m_history += "Solve tetrahedron\n";
    PrintState();

    FixWinding();
    float dots[4];
    Test test[4];
    Vector3 bestPoint;
    int bestIndex = -1;
    for(int i = 0; i < 4; ++i)
    {
      Vector3 a, b, c;
      Vector3 o = GetTri(i, a, b, c);
      dots[i] = TriangleNormal(a, b, c).Dot(-a);
      float s,t;
      bool clamped;
      Vector3 closest = ClosestOnTri(Vector3::Zero, a, b, c, &s, &t, &clamped);
      if(dots[i] && (bestIndex == -1 || closest.Length2() < bestPoint.Length2()))
      {
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
    for(int i = 0; i < 4; ++i)
    {
      Vector3 a, b, c;
      Vector3 other = GetTri(i, a, b, c);
      if(TriangleNormal(a, b, c).Dot(-a) > 0.0f)
      {
        inFront = true;
        bool wasClamped;
        ClosestOnTri(Vector3::Zero, a, b, c, nullptr, nullptr, &wasClamped);
        if(wasClamped)
          continue;

        Discard(other);
        Vector3 result = SolveTriangle();
        if(!result.Equal(-bestPoint, 0.01f))
          __debugbreak();
        SyxAssertError(result.Equal(-bestPoint, 0.01f), "Wrong point");
        return result;
      }
    }
    // In front of one or more faces, but not contained by any
    if(inFront)
    {
      Vector3 a, b, c;
      Discard(GetTri(bestIndex, a, b, c));
      Vector3 result = SolveTriangle();
      SyxAssertError(result.Equal(-bestPoint, 0.01f), "Wrong point");
      return result;
    }

    return Vector3::Zero;
  }

  void DebugSimplex::Discard(const Vector3& point)
  {
    for(size_t i = 0; i < m_size; ++i)
      if(point == Get(i))
      {
        Discard(i);
        return;
      }
    SyxAssertError(false, "Tried to discard point that wasn't in simplex");
  }

  void DebugSimplex::Discard(int id)
  {
    m_history += "Discard " + GetIndexName(id);
    m_history += "\n";

    --m_size;
    for(size_t i = static_cast<size_t>(id); i < m_size; ++i)
      m_supports[i] = m_supports[i + 1];
  }

  void DebugSimplex::Discard(int a, int b)
  {
    if(a > b)
      std::swap(a, b);
    Discard(b);
    Discard(a);
  }

  void DebugSimplex::Discard(int a, int b, int c)
  {
    OrderAscending(a, b, c);
    Discard(c);
    Discard(b);
    Discard(a);
  }

  void DebugSimplex::FixWinding(void)
  {
    Vector3 normal = TriangleNormal(Get(SupportID::A), Get(SupportID::B), Get(SupportID::C));
    float dot = normal.Dot(-Get(SupportID::A));
    if(dot > 0.0f)
    {
      std::swap(GetSupport(SupportID::B), GetSupport(SupportID::C));
      m_history += "Fixed winding\n";
    }
  }

  void DebugSimplex::PrintState(void)
  {
    m_history += "Simplex Points:\n";

    for(size_t i = 0; i < m_size; ++i)
    {
      const Vector3& p = Get(i);
      m_history += GetIndexName(i) + ": (" + std::to_string(p.x) + ", " + std::to_string(p.y) + ", " + std::to_string(p.z) + ")\n";
    }
  }

  std::string DebugSimplex::GetIndexName(int index)
  {
    switch(index)
    {
    case 0: return "A";
    case 1: return "B";
    case 2: return "C";
    case 3: return "D";
    //I for invalid
    default: return "I";
    }
  }

  bool DebugSimplex::operator==(const Simplex& rhs)
  {
    if(m_size != rhs.Size())
      return false;

    for(size_t i = 0; i < m_size; ++i)
      if(Get(i) != rhs.Get(i))
        return false;
    return true;
  }

  bool DebugSimplex::operator!=(const Simplex& rhs)
  {
    return !(*this == rhs);
  }

  DebugSimplex& DebugSimplex::operator=(const Simplex& rhs)
  {
    m_size = rhs.Size();
    for(size_t i = 0; i < m_size; ++i)
      m_supports[i] = rhs.GetSupport(i);
    return *this;
  }
}