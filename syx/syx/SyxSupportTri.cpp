#include "Precompile.h"
#include "SyxSupportTri.h"
#include "SyxSimplex.h"

namespace Syx {
  SupportTri::SupportTri(void) {
    mVerts[0] = mVerts[1] = mVerts[2] = -1;
  }

  SupportTri::SupportTri(int a, int b, int c, SupportPoints& pts) : mVerts{ a, b, c } {
    Vec3 pa = pts[mVerts[0]].mSupport;
    Vec3 pb = pts[mVerts[1]].mSupport;
    Vec3 pc = pts[mVerts[2]].mSupport;

    mHalfPlane = triangleNormal(pa, pb, pc).safeNormalized();
    mHalfPlane.w = -pa.dot(mHalfPlane);
  }

  SupportTri::SupportTri(int a, int b, int c, const Vec3& halfPlane) : mVerts{ a, b, c }, mHalfPlane(halfPlane) {}

  float SupportTri::signedNormalDist(const Vec3& point) const {
    return mHalfPlane.dot4(point);
  }

  float SupportTri::originDist(void) const {
    return mHalfPlane.w;
  }

  Vec3 SupportTri::project(const Vec3& point) const {
    float dist = signedNormalDist(point);
    return point - mHalfPlane*dist;
  }
}