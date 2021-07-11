#include "Precompile.h"
#include "SyxSupportTri.h"
#include "SyxSimplex.h"

namespace Syx {
  SupportTri::SupportTri(void) {
    mVerts[0] = mVerts[1] = mVerts[2] = SupportID::None;
  }

  SupportTri::SupportTri(SupportID a, SupportID b, SupportID c, SupportPoints& pts) : mVerts{ a, b, c } {
    Vec3 pa = pts[size_t(mVerts[0])].mSupport;
    Vec3 pb = pts[size_t(mVerts[1])].mSupport;
    Vec3 pc = pts[size_t(mVerts[2])].mSupport;

    mHalfPlane = triangleNormal(pa, pb, pc).safeNormalized();
    mHalfPlane.w = -pa.dot(mHalfPlane);
  }

  SupportTri::SupportTri(SupportID a, SupportID b, SupportID c, const Vec3& halfPlane) : mVerts{ a, b, c }, mHalfPlane(halfPlane) {}

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