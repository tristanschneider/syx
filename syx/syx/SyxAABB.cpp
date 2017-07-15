#include "Precompile.h"
#include "SyxAABB.h"
#include "SyxTransform.h"

namespace Syx {

#ifdef SENABLED
  void SAABB::Construct(const Vector3Vec& points, bool clear) {
    if(points.empty())
      return;

    if(clear)
      mMin = mMax = SLoadAll(&points[0].x);
    for(size_t i = 1; i < points.size(); ++i) {
      SFloats p = SLoadAll(&points[i].x);
      mMin = SMinAll(mMin, p);
      mMax = SMaxAll(mMax, p);
    }
  }

  SFloats SAABB::GetVolume(void) const {
    return SVector3::Mul3(GetDiagonal());
  }

  SFloats SAABB::GetInertia(void) const {
    SFloats dimensions = GetDiagonal();
    //x*y*z in all elements
    SFloats mass = SMulAll(dimensions, SShuffle(dimensions, 1, 2, 0, 3));
    mass = SMulAll(mass, SShuffle(dimensions, 2, 0, 1, 3));

    static const SFloats div12 = SLoadFloats(1.0f/12.0f, 1.0f/12.0f, 1.0f/12.0f, 0.0f);
    mass = SMulAll(mass, div12);

    dimensions = SMulAll(dimensions, dimensions);

    //height = y, width = z, length = x
    //Ixx = m/12 * (h^2+w^2)
    //Iyy = m/12 * (l^2+w^2)
    //Izz = m/12 * (h^2+l^2)
    return SMulAll(mass, SAddAll(SShuffle(dimensions, 1, 0, 1, 3), SShuffle(dimensions, 2, 2, 0, 3)));
  }
#endif

  void AABB::Construct(const Vector3Vec& points, bool clear) {
    if(points.empty())
      return;

    if(clear)
      mMin = mMax = points[0];
    for(size_t i = 1; i < points.size(); ++i)
      for(int j = 0; j < 3; ++j) {
        float val = points[i][j];
        if(val < mMin[j])
          mMin[j] = val;
        if(val > mMax[j])
          mMax[j] = val;
      }
  }

  float AABB::GetVolume(void) const {
    Vector3 d = GetDiagonal();
    return d.x*d.y*d.z;
  }

  Vector3 AABB::GetInertia(void) const {
    //height = y, width = z, length = x
    //Ixx = m/12 * (h^2+w^2)
    //Iyy = m/12 * (l^2+w^2)
    //Izz = m/12 * (h^2+l^2)
    Vector3 d = GetDiagonal();
    float m12 = (d.x*d.y*d.z)/12.0f;
    float heightSq = d.y*d.y;
    float widthSq = d.z*d.z;
    float lengthSq = d.x*d.x;

    return Vector3(m12 * (heightSq + widthSq),
      m12 * (lengthSq + widthSq),
      m12 * (heightSq + lengthSq));
  }

  bool AABB::LineIntersect(const Vector3& start, const Vector3& end, float* resultT, int* normalIndex, int* normalSign) const {
    Vector3 dirFrac = (end - start).Reciprocal();
    //Smallest and largest intersection time between line and box, so where the infinite ray enters and leaves it
    //Start at zero as this will remain unchanged if the line is within the box
    float tMin = 0.0f;
    float tMax = std::numeric_limits<float>::max();
    //So I don't need to re-check them in the loop, make sure they're non-null
    int normalIndexDefault, normalSignDefault;
    normalIndex = normalIndex ? normalIndex : &normalIndexDefault;
    normalSign = normalSign ? normalSign : &normalSignDefault;
    //Arbitrary axis and sign for when ray is inside box
    *normalIndex = *normalSign = 1;

    //Compute intersect Ts with slabs separated into near and far planes
    for(int i = 0; i < 3; ++i) {
      if(std::abs(dirFrac[i]) > SYX_EPSILON) {
        float curMin = (mMin[i] - start[i])*dirFrac[i];
        float curMax = (mMax[i] - start[i])*dirFrac[i];
        bool swapped = false;
        if(curMin > curMax) {
          std::swap(curMin, curMax);
          swapped = true;
        }
        if(curMin > tMin) {
          tMin = curMin;
          *normalIndex = i;
          *normalSign = swapped ? 1 : -1;
        }
        tMax = std::min(curMax, tMax);
      }
      //No change on this axis, so just need to make sure it is within box on this axis
      else if(!Between(start[i], mMin[i], mMax[i]))
        return false;
    }

    //if tMax < 0, ray (line) is intersecting AABB, but whole AABB is behind us
    //if tMin > tMax, ray doesn't intersect AABB, as not all axes were overlapping at the same time
    //if tMax > length ray doesn't go far enough to intersect
    //if tMin < 0 start point is inside
    if(tMax < 0 || tMin > tMax || tMin > 1.0f)
      return false;
    if(resultT)
      *resultT = std::max(0.0f, tMin);
    return true;
  }

  //UpdateAABB from Realtime Collision Detection
  AABB AABB::Transform(const Transformer& transformer) const {
    AABB result;

    for(int i = 0; i < 3; ++i) {
      result.mMin[i] = result.mMax[i] = transformer.mPos[i];
      for(int j = 0; j < 3; ++j) {
        float min = transformer.mScaleRot[j][i]*mMin[j];
        float max = transformer.mScaleRot[j][i]*mMax[j];
        if(min > max)
          std::swap(min, max);
        result.mMin[i] += min;
        result.mMax[i] += max;
      }
    }
    return result;
  }

  void AABB::Draw(void) const {
    DebugDrawer::Get().DrawCube(GetCenter(), GetDiagonal(), Vector3::UnitX, Vector3::UnitY);
  }

  AABB AABB::Combined(const AABB& lhs, const AABB& rhs) {
    AABB result;
    for(int i = 0; i < 3; ++i) {
      result.mMin[i] = std::min(lhs.mMin[i], rhs.mMin[i]);
      result.mMax[i] = std::max(lhs.mMax[i], rhs.mMax[i]);
    }
    return result;
  }

  float AABB::GetSurfaceArea(void) const {
    Vector3 dims = mMax - mMin;
    return dims.x*dims.y + dims.x*dims.z + dims.y*dims.z;
  }

  bool AABB::Overlapping(const AABB& other) const {
    //Loop over each axis hoping for an early out to avoid computations,
    //as this is heavily used for broadphase calculation
    for(int i = 0; i < 3; ++i) {
      float mySize = (mMax[i] - mMin[i])*0.5f;
      float myCenter = mMin[i] + mySize;
      float otherSize = (other.mMax[i] - other.mMin[i])*0.5f;
      float otherCenter = other.mMin[i] + otherSize;
      if(std::abs(myCenter - otherCenter) > mySize + otherSize)
        return false;
    }
    return true;
  }

  bool Within(float test, float min, float max) {
    return test >= min && test <= max;
  }

  bool AABB::IsInside(const Vector3& point) const {
    return Within(point.x, mMin.x, mMax.x) &&
      Within(point.y, mMin.y, mMax.y) &&
      Within(point.z, mMin.z, mMax.z);
  }

  bool AABB::IsInside(const AABB& aabb) const {
    return IsInside(aabb.mMin) && IsInside(aabb.mMax);
  }

  void AABB::Pad(float padPercent) {
    Vector3 pad = (mMax - mMin)*padPercent;
    mMin -= pad;
    mMax += pad;
  }

  void AABB::Move(const Vector3& amount) {
    mMin += amount;
    mMax += amount;
  }

  void AABB::Add(const Vector3& point) {
    for(int i = 0; i < 3; ++i) {
      mMin[i] = std::min(mMin[i], point[i]);
      mMax[i] = std::max(mMax[i], point[i]);
    }
  }

  void AABB::Init(const Vector3& point) {
    mMin = mMax = point;
  }
}