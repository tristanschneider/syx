#include "Precompile.h"
#include "SyxDebugHelpers.h"

namespace Syx {
  //I'll deal with the normal later when I actually want to use it
  void drawTriangle(const Vec3& a, const Vec3& b, const Vec3& c, bool drawNormal) {
    DebugDrawer& d = DebugDrawer::get();
    d.drawLine(a, b);
    d.drawLine(b, c);
    d.drawLine(c, a);
    if (drawNormal)
      d.drawVector((a + b + c)*1.0f / 3.0f, triangleNormal(a, b, c).safeNormalized());
  }

  void drawSphere(const Vec3& center, float radius) {
    DebugDrawer::get().drawSphere(center, radius, Vec3::UnitX, Vec3::UnitY);
  }

  void drawCube(const Vec3& center, float scale) {
    DebugDrawer::get().drawCube(center, Vec3(scale), Vec3::UnitX, Vec3::UnitY);
  }

  void drawCapsule(const Vec3& pos, const Mat3& scaleRot) {
    Vec3 top = pos + scaleRot.mby;
    Vec3 bottom = pos - scaleRot.mby;
    const Vec3& x = scaleRot.mbx;
    const Vec3& z = scaleRot.mbz;
    DebugDrawer& d = DebugDrawer::get();
    Vec3 tops[] = { top + x, top - x, top + z, top - z };
    Vec3 bottoms[] = { bottom + x, bottom - x, bottom + z, bottom - z};
    for(int i = 0; i < 4; ++i)
      d.drawLine(tops[i], bottoms[i]);
    Vec3 zn = z.safeNormalized();
    Vec3 xn = x.safeNormalized();
    d.drawArc(top, x, zn, SYX_PI);
    d.drawArc(top, z, -xn, SYX_PI);
    d.drawArc(bottom, x, -zn, SYX_PI);
    d.drawArc(bottom, z, xn, SYX_PI);
  }
};