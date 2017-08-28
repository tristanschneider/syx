#pragma once
namespace Syx {
  struct Vec3;
  struct Mat3;

  void drawTriangle(const Vec3& a, const Vec3& b, const Vec3& c, bool drawNormal = false);
  void drawSphere(const Vec3& center, float radius);
  void drawCube(const Vec3& center, float scale);
  void drawCapsule(const Vec3& pos, const Mat3& scaleRot);
}