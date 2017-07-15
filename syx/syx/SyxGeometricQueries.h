#pragma once

namespace Syx {
  struct Vector3;
  //CCW ABC is assumed to be pointing outwards
  Vector3 ClosestOnTetrahedron(const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& d, const Vector3& point);
  Vector3 ClosestOnRay(const Vector3& p, const Vector3& a, const Vector3& b);
  Vector3 ClosestOnLine(const Vector3& p, const Vector3& a, const Vector3& b);
  Vector3 ClosestOnTri(const Vector3& p, const Vector3& a, const Vector3& b, const Vector3& c, float* resultS = nullptr, float* resultT = nullptr, bool* clamped = nullptr);
  Vector3 ClosestOnTriFromEdges(const Vector3& triA, const Vector3& pToA, const Vector3& aToB, const Vector3& aToC, float* resultABT = nullptr, float* resultACT = nullptr, bool* clamped = nullptr);
  float PointRayDist2(const Vector3& p, const Vector3& a, const Vector3& b);
  float PointLineDist2(const Vector3& p, const Vector3& a, const Vector3& b);
  void ClosestOnRays(const Vector3& aStart, const Vector3& aDir, const Vector3& bStart, const Vector3& bDir, float& ta, float& tb);
  float TriangleLineIntersect(const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& start, const Vector3& end);
}
