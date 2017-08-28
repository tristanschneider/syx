#pragma once

namespace Syx {
  struct Vec3;
  //CCW ABC is assumed to be pointing outwards
  Vec3 closestOnTetrahedron(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, const Vec3& point);
  Vec3 closestOnRay(const Vec3& p, const Vec3& a, const Vec3& b);
  Vec3 closestOnLine(const Vec3& p, const Vec3& a, const Vec3& b);
  Vec3 closestOnTri(const Vec3& p, const Vec3& a, const Vec3& b, const Vec3& c, float* resultS = nullptr, float* resultT = nullptr, bool* clamped = nullptr);
  Vec3 closestOnTriFromEdges(const Vec3& triA, const Vec3& pToA, const Vec3& aToB, const Vec3& aToC, float* resultABT = nullptr, float* resultACT = nullptr, bool* clamped = nullptr);
  float pointRayDist2(const Vec3& p, const Vec3& a, const Vec3& b);
  float pointLineDist2(const Vec3& p, const Vec3& a, const Vec3& b);
  void closestOnRays(const Vec3& aStart, const Vec3& aDir, const Vec3& bStart, const Vec3& bDir, float& ta, float& tb);
  float triangleLineIntersect(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& start, const Vec3& end);
}
