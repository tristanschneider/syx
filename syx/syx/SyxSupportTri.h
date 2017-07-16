#pragma once
#include <vector>
#include "SyxAlignmentAllocator.h"
#include "SyxSimplex.h"

namespace Syx
{
  struct SupportPoint;

  struct SupportEdge
  {
    SupportEdge(void): m_from(-1), m_to(-1) {}
    SupportEdge(int from, int to): m_from(from), m_to(to) {}

    int m_from, m_to;
  };

  typedef std::vector<SupportPoint, AlignmentAllocator<SupportPoint>> SupportPoints;

  SAlign struct SupportTri
  {
    int m_verts[3];
    int m_padding;
    SAlign Vec3 m_halfPlane;

    SupportTri(void);
    SupportTri(int a, int b, int c, SupportPoints& pts);
    SupportTri(int a, int b, int c, const Vec3& halfPlane);

    float SignedNormalDist(const Vec3& point) const;
    float OriginDist(void) const;
    Vec3 Project(const Vec3& point) const;

    template <typename Container>
    void AddEdges(Container& container) const
    {
      container.emplace_back(m_verts[0], m_verts[1]);
      container.emplace_back(m_verts[1], m_verts[2]);
      container.emplace_back(m_verts[2], m_verts[0]);
    }
  };
};