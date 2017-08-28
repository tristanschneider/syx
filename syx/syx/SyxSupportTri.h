#pragma once
#include "SyxSimplex.h"

namespace Syx {
  struct SupportPoint;

  struct SupportEdge {
    SupportEdge(void) : mFrom(-1), mTo(-1) {}
    SupportEdge(int from, int to) : mFrom(from), mTo(to) {}

    int mFrom, mTo;
  };

  typedef std::vector<SupportPoint, AlignmentAllocator<SupportPoint>> SupportPoints;

  SAlign struct SupportTri {
    int mVerts[3];
    int mPadding;
    SAlign Vec3 mHalfPlane;

    SupportTri(void);
    SupportTri(int a, int b, int c, SupportPoints& pts);
    SupportTri(int a, int b, int c, const Vec3& halfPlane);

    float signedNormalDist(const Vec3& point) const;
    float originDist(void) const;
    Vec3 project(const Vec3& point) const;

    template <typename Container>
    void addEdges(Container& container) const {
      container.emplace_back(mVerts[0], mVerts[1]);
      container.emplace_back(mVerts[1], mVerts[2]);
      container.emplace_back(mVerts[2], mVerts[0]);
    }
  };
};