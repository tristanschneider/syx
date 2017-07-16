#pragma once

namespace Syx {
  struct Vec3;
  //CCW ABC is assumed to be pointing outwards
  Vec3 ClosestOnTetrahedron(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, const Vec3& point);
  Vec3 ClosestOnRay(const Vec3& p, const Vec3& a, const Vec3& b);
  Vec3 ClosestOnLine(const Vec3& p, const Vec3& a, const Vec3& b);
  Vec3 ClosestOnTri(const Vec3& p, const Vec3& a, const Vec3& b, const Vec3& c, float* resultS = nullptr, float* resultT = nullptr, bool* clamped = nullptr);
  Vec3 ClosestOnTriFromEdges(const Vec3& triA, const Vec3& pToA, const Vec3& aToB, const Vec3& aToC, float* resultABT = nullptr, float* resultACT = nullptr, bool* clamped = nullptr);
  float PointRayDist2(const Vec3& p, const Vec3& a, const Vec3& b);
  float PointLineDist2(const Vec3& p, const Vec3& a, const Vec3& b);
  void ClosestOnRays(const Vec3& aStart, const Vec3& aDir, const Vec3& bStart, const Vec3& bDir, float& ta, float& tb);
  float TriangleLineIntersect(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& start, const Vec3& end);
}
