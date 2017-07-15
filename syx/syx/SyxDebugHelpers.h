#pragma once
#include "SyxVector3.h"

namespace Syx {
  void DrawTriangle(const Vector3& a, const Vector3& b, const Vector3& c, bool drawNormal = false);
  void DrawSphere(const Vector3& center, float radius);
  void DrawCube(const Vector3& center, float scale);
  void DrawCapsule(const Vec3& pos, const Mat3& scaleRot);
}