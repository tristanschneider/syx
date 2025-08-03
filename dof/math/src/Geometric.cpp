#include <math/Geometric.h>

namespace Geo {
  bool unitAABBLineIntersect(const glm::vec2& origin, const glm::vec2& dir, float* resultTIn, float* resultTOut) {
    float tMin = 0.0f;
    float tMax = std::numeric_limits<float>::max();
    //Compute intersect Ts with slabs separated into near and far planes
    for(int i = 0; i < 2; ++i) {
      constexpr float aabbMin = -0.5f;
      constexpr float aabbMax = 0.5f;

      if(std::abs(dir[i]) > 0.00001f) {
        const float recip = 1.0f/dir[i];
        float curMin = (aabbMin - origin[i])*recip;
        float curMax = (aabbMax - origin[i])*recip;
        if(curMin > curMax) {
          std::swap(curMin, curMax);
        }
        if(curMin > tMin) {
          tMin = curMin;
        }
        tMin = std::max(curMin, tMin);
        tMax = std::min(curMax, tMax);
      }
      //No change on this axis, so just need to make sure it is within box on this axis
      else if(!Geo::between(origin[i], aabbMin, aabbMax)) {
        return false;
      }
    }

    //if tMax < 0, ray (line) is intersecting AABB, but whole AABB is behind us
    //if tMin > tMax, ray doesn't intersect AABB, as not all axes were overlapping at the same time
    //if tMax > length ray doesn't go far enough to intersect
    //if tMin < 0 start point is inside
    if(tMax < 0 || tMin > tMax) {
      return false;
    }
    if(resultTIn) {
      *resultTIn = std::max(0.0f, tMin);
    }
    if(resultTOut) {
      *resultTOut = tMax;
    }
    return true;
  }

  //Since it's a unit cube the extents are at 0.5 in each direction.
  //Intersects would be on the edge but the point could also be entirely inside the shape
  //In either case the normal is the most extreme axis
  //Exact center would hit one of the cases, any of which are valid because all normals are equally far to the surface
  glm::vec2 getNormalFromUnitAABBIntersect(const glm::vec2& intersect) {
    if(intersect.x > 0.0f) {
      //Top right quadrant
      if(intersect.y > 0.0f) {
        return intersect.x > intersect.y ? glm::vec2{ 1, 0 } : glm::vec2{ 0, 1 };
      }
      //Bottom right quadrant
      return intersect.x > -intersect.y ? glm::vec2{ 1, 0 } : glm::vec2{ 0, -1 };
    }
    //Top left quadrant
    if(intersect.y > 0.0f) {
      return -intersect.x > intersect.y ? glm::vec2{ -1, 0 } : glm::vec2{ 0, 1 };
    }
    //Bottom left quadrant
    return intersect.x < intersect.y ? glm::vec2{ -1, 0 } : glm::vec2{ 0, -1 };
  }
}