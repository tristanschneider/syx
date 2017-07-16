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

  Vec3 DebugSimplex::Solve(void)
  {
    switch(m_size)
    {
    case 1: m_history += "Solve point\n"; return -m_supports[0].mSupport;
    case 2: return SolveLine();
    case 3: return SolveTriangle();
    case 4: return SolveTetrahedron();
    default: SyxAssertError(false, "Nonsense size in debug solve"); return Vec3::Zero;
    }
  }

  void DebugSimplex::GrowToFourPoints(Narrowphase& narrow)
  {
    //Possible directions to look for support points
    static SAlign Vec3 searchDirs[] =
    {
      Vec3::UnitX, Vec3::UnitY, Vec3::UnitZ,
      -Vec3::UnitX, -Vec3::UnitY, -Vec3::UnitZ,
    };

    //Intentional fall through cases
    switch(m_size)
    {
    //Already has four, get out of here
    case 4:
      return;

    case 0:
      //Arbitrary direction. Doesn't matter since there are no others for it to be a duplicate of
      Add(narrow.GetSupport(Vec3::UnitY), false);

    case 1:
      for(Vec3& curDir : searchDirs)
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
      Vec3 lineSeg = (m_supports[1].mSupport - m_supports[0].mSupport).SafeNormalized();
      int leastSignificantAxis = lineSeg.LeastSignificantAxis();
      SAlign Vec3 searchDir = lineSeg.Cross(searchDirs[leastSignificantAxis]);
      //Matrix would be a bit faster, but I don't imagine this case comes
      //up often enough for it to matter
      Quat rot = Quat::AxisAngle(lineSeg, 3.14f/3.0f);
      SupportPoint newPoint = narrow.GetSupport(searchDir);

      for(unsigned i = 0; i < 6; ++i)
      {
        SupportPoint curPoint = narrow.GetSupport(searchDir);
        if(Vec3::PointLineDistanceSQ(curPoint.mSupport, m_supports[0].mSupport, m_supports[1].mSupport) > SYX_EPSILON)
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
      SAlign Vec3 searchDir = TriangleNormal(m_supports[2].mSupport,
        m_supports[1].mSupport, m_supports[0].mSupport);

      SupportPoint newPoint = narrow.GetSupport(searchDir);
      //If this point matches one of the other points already, search in a different direction
      for(unsigned i = 0; i < 3; ++i)
        if(m_supports[i].mSupport == newPoint.mSupport)
        {
          //For flat shapes this could still result in a duplicate, but we can't do anything better from here
          SAlign Vec3 negDir = -searchDir;
          newPoint = narrow.GetSupport(negDir);
          break;
        }

      Add(newPoint, false);
    }
    }

    //Fix winding
    Vec3 v30 = m_supports[0].mSupport - m_supports[3].mSupport;
    Vec3 v31 = m_supports[1].mSupport - m_supports[3].mSupport;
    Vec3 v32 = m_supports[2].mSupport - m_supports[3].mSupport;
    float det = Vec3::Dot(v30, Vec3::Cross(v31, v32));
    if(det <= 0.0f)
      std::swap(m_supports[0], m_supports[1]);
  }

  bool DebugSimplex::Contains(const Vec3& support) const
  {
    for(size_t i = 0; i < m_size; ++i)
      if(m_supports[i].mSupport == support)
        return true;
    return false;
  }

  Vec3 DebugSimplex::GetTri(int index, Vec3& ra, Vec3& rb, Vec3& rc) const
  {
    // B.
    // | C
    // |/   D
    // A
    const Vec3& a = m_supports[0].mSupport;
    const Vec3& b = m_supports[1].mSupport;
    const Vec3& c = m_supports[2].mSupport;
    const Vec3& d = m_supports[3].mSupport;
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
    default: SyxAssertError(false, "Nonsense index"); return Vec3::Zero;
    }
  }

  bool DebugSimplex::ContainsOrigin(void)
  {
    if(IsDegenerate())
      SyxAssertError(false, "Don't want to deal with figuring out if degenerate shape contains origin");

    Vec3 o = Vec3::Zero;
    const Vec3& a = Get(SupportID::A);
    const Vec3& b = Get(SupportID::B);
    const Vec3& c = Get(SupportID::C);

    switch(m_size)
    {
      case 1: return a == o;
      case 2:
      {
        Vec3 ao = o - a;
        Vec3 ab = b - a;
        float t = Vec3::ProjVecScalar(ao, ab);
        if(t < 0.0f || t > 1.0f)
          return false;
        return o == a + ab*t;
      }
      case 3: return ClosestOnTri(o, a, b, c) == o;
      case 4:
      {
        FixWinding();
        Vec3 ta, tb, tc;
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
    const Vec3& a = Get(SupportID::A);
    const Vec3& b = Get(SupportID::B);
    const Vec3& c = Get(SupportID::C);
    const Vec3& d = Get(SupportID::D);

    switch(m_size)
    {
    case 1: return false;
    case 2: return a == b;
    case 3: return TriangleNormal(a, b, c).Length() < SYX_EPSILON;
    case 4: return std::abs((d - a).Dot(TriangleNormal(a, b, c))) < SYX_EPSILON;
    default: SyxAssertError(false, "Nonsense size"); return false;
    }
  }

  bool DebugSimplex::MakesProgress(const Vec3& newPoint, const Vec3& searchDir) const
  {
    float newDot = newPoint.Dot(searchDir);
    for(size_t i = 0; i < m_size; ++i)
      if(m_supports[i].mSupport.Dot(searchDir) >= newDot)
        return false;
    return true;
  }

  Vec3 DebugSimplex::SolveLine(void)
  {
    m_history += "Solve line\n";
    PrintState();

    Vec3 a = Get(SupportID::A);
    Vec3 b = Get(SupportID::B);

    Vec3 aToB = b - a;
    float proj = Vec3::ProjVecScalar(-a, aToB);
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

  Vec3 DebugSimplex::SolveTriangle(void)
  {
    m_history += "Solve triangle\n";
    PrintState();

    Vec3 a = Get(SupportID::A);
    Vec3 b = Get(SupportID::B);
    Vec3 c = Get(SupportID::C);
    //Vec3 n = TriangleNormal(a, b, c);
    //
    //Vec3 abNorm = (b - a).Cross(n).SafeNormalized();
    //Vec3 bcNorm = (c - b).Cross(n).SafeNormalized();
    //Vec3 caNorm = (a - c).Cross(n).SafeNormalized();
    //bool lineCase = true;
    //Vec3 lineResult;
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
    Vec3 result = -ClosestOnTri(Vec3::Zero, a, b, c, &s, &t, &wasClamped);
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
      Vec3 test = Solve();
      SyxAssertError(test.Equal(result, 0.01f), "Wrong result");
    }
    //SyxAssertError(!lineCase || result == SolveLine(), "Made a mistake with barycentric discarding");

    //Vec3 result = -ClosestOnTri(Vec3::Zero, a, b, c, &s, &t);
    //if(lineCase && lineResult != result)
    //  __debugbreak();
    //SyxAssertError(!lineCase || lineResult == result, "Results don't match, indicating bogus");
    return result;
  }

  struct Test
  {
    Test(float inS, float inT, const Vec3& inP, bool b): s(inS), t(inT), p(inP), clamped(b) {}
    Test(void) {}

    float s, t;
    Vec3 p, o;
    bool clamped;
    float oproj;
  };

  Vec3 DebugSimplex::SolveTetrahedron(void)
  {
    m_history += "Solve tetrahedron\n";
    PrintState();

    FixWinding();
    float dots[4];
    Test test[4];
    Vec3 bestPoint;
    int bestIndex = -1;
    for(int i = 0; i < 4; ++i)
    {
      Vec3 a, b, c;
      Vec3 o = GetTri(i, a, b, c);
      dots[i] = TriangleNormal(a, b, c).Dot(-a);
      float s,t;
      bool clamped;
      Vec3 closest = ClosestOnTri(Vec3::Zero, a, b, c, &s, &t, &clamped);
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
      Vec3 a, b, c;
      Vec3 other = GetTri(i, a, b, c);
      if(TriangleNormal(a, b, c).Dot(-a) > 0.0f)
      {
        inFront = true;
        bool wasClamped;
        ClosestOnTri(Vec3::Zero, a, b, c, nullptr, nullptr, &wasClamped);
        if(wasClamped)
          continue;

        Discard(other);
        Vec3 result = SolveTriangle();
        if(!result.Equal(-bestPoint, 0.01f))
          __debugbreak();
        SyxAssertError(result.Equal(-bestPoint, 0.01f), "Wrong point");
        return result;
      }
    }
    // In front of one or more faces, but not contained by any
    if(inFront)
    {
      Vec3 a, b, c;
      Discard(GetTri(bestIndex, a, b, c));
      Vec3 result = SolveTriangle();
      SyxAssertError(result.Equal(-bestPoint, 0.01f), "Wrong point");
      return result;
    }

    return Vec3::Zero;
  }

  void DebugSimplex::Discard(const Vec3& point)
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
    Vec3 normal = TriangleNormal(Get(SupportID::A), Get(SupportID::B), Get(SupportID::C));
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
      const Vec3& p = Get(i);
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