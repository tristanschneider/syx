#pragma once
namespace Syx {
  struct Vec3;
  struct Mat3;

  void DrawTriangle(const Vec3& a, const Vec3& b, const Vec3& c, bool drawNormal = false);
  void DrawSphere(const Vec3& center, float radius);
  void DrawCube(const Vec3& center, float scale);
  void DrawCapsule(const Vec3& pos, const Mat3& scaleRot);
}