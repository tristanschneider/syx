#include "Precompile.h"
#include "SyxDebugHelpers.h"

namespace Syx {
  //I'll deal with the normal later when I actually want to use it
  void DrawTriangle(const Vec3& a, const Vec3& b, const Vec3& c, bool drawNormal) {
    DebugDrawer& d = DebugDrawer::Get();
    d.DrawLine(a, b);
    d.DrawLine(b, c);
    d.DrawLine(c, a);
    if (drawNormal)
      d.DrawVector((a + b + c)*1.0f / 3.0f, TriangleNormal(a, b, c).SafeNormalized());
  }

  void DrawSphere(const Vec3& center, float radius) {
    DebugDrawer::Get().DrawSphere(center, radius, Vec3::UnitX, Vec3::UnitY);
  }

  void DrawCube(const Vec3& center, float scale) {
    DebugDrawer::Get().DrawCube(center, Vec3(scale), Vec3::UnitX, Vec3::UnitY);
  }

  void DrawCapsule(const Vec3& pos, const Mat3& scaleRot) {
    Vec3 top = pos + scaleRot.mby;
    Vec3 bottom = pos - scaleRot.mby;
    const Vec3& x = scaleRot.mbx;
    const Vec3& z = scaleRot.mbz;
    DebugDrawer& d = DebugDrawer::Get();
    Vec3 tops[] = { top + x, top - x, top + z, top - z };
    Vec3 bottoms[] = { bottom + x, bottom - x, bottom + z, bottom - z};
    for(int i = 0; i < 4; ++i)
      d.DrawLine(tops[i], bottoms[i]);
    Vec3 zn = z.SafeNormalized();
    Vec3 xn = x.SafeNormalized();
    d.DrawArc(top, x, zn, SYX_PI);
    d.DrawArc(top, z, -xn, SYX_PI);
    d.DrawArc(bottom, x, -zn, SYX_PI);
    d.DrawArc(bottom, z, xn, SYX_PI);
  }
};