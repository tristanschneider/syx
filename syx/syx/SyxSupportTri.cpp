#include "Precompile.h"
#include "SyxSupportTri.h"
#include "SyxSimplex.h"

namespace Syx {
  SupportTri::SupportTri(void) {
    m_verts[0] = m_verts[1] = m_verts[2] = -1;
  }

  SupportTri::SupportTri(int a, int b, int c, SupportPoints& pts) : m_verts{ a, b, c } {
    Vec3 pa = pts[m_verts[0]].mSupport;
    Vec3 pb = pts[m_verts[1]].mSupport;
    Vec3 pc = pts[m_verts[2]].mSupport;

    m_halfPlane = TriangleNormal(pa, pb, pc).SafeNormalized();
    m_halfPlane.w = -pa.Dot(m_halfPlane);
  }

  SupportTri::SupportTri(int a, int b, int c, const Vec3& halfPlane) : m_verts{ a, b, c }, m_halfPlane(halfPlane) {}

  float SupportTri::SignedNormalDist(const Vec3& point) const {
    return m_halfPlane.Dot4(point);
  }

  float SupportTri::OriginDist(void) const {
    return m_halfPlane.w;
  }

  Vec3 SupportTri::Project(const Vec3& point) const {
    float dist = SignedNormalDist(point);
    return point - m_halfPlane*dist;
  }
}