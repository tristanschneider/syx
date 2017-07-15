#pragma once
#include "SyxVector3.h"
#include "SyxSVector3.h"
#include "SyxAlignmentAllocator.h"
#include <vector>

namespace Syx {
  struct Transformer;

#ifdef SENABLED
  class SAABB {
  public:
    SAABB(SFloats min, SFloats max): mMin(min), mMax(max) {}
    SAABB(void) {}
    SAABB(const Vector3Vec& points) { Construct(points, true); }

    void Construct(const Vector3Vec& points, bool clear);

    SFloats GetVolume(void) const;
    SFloats GetInertia(void) const;

    SFloats GetMin(void) const { return mMin; }
    void SetMin(SFloats min) { mMin = min; }
    SFloats GetMax(void) const { return mMax; }
    void SetMax(SFloats max) { mMax = max; }
    SFloats GetDiagonal(void) const { return SSubAll(mMax, mMin); }

  private:
    //Not to be touched directly so I can easily do a different representation if it proves to be better
    SFloats mMin;
    SFloats mMax;
  };
#endif

  class AABB {
  public:
    AABB(const Vector3& min, const Vector3& max): mMin(min), mMax(max) {}
    AABB(void) {}
    AABB(const Vector3Vec& points) { Construct(points, true); }

    void Construct(const Vector3Vec& points, bool clear);

    float GetVolume(void) const;
    Vector3 GetInertia(void) const;
    float GetSurfaceArea(void) const;

    Vector3 GetMin(void) const { return mMin; }
    void SetMin(const Vector3& min) { mMin = min; }
    Vector3 GetMax(void) const { return mMax; }
    void SetMax(const Vector3& max) { mMax = max; }
    Vector3 GetDiagonal(void) const { return mMax - mMin; }
    Vector3 GetCenter(void) const { return (mMin + mMax)*0.5f; }
    bool Overlapping(const AABB& other) const;
    bool IsInside(const Vector3& point) const;
    bool IsInside(const AABB& aabb) const;
    void Pad(float padPercent);
    void Move(const Vector3& amount);
    void Add(const Vector3& point);
    void Init(const Vector3& point);

    AABB Transform(const Transformer& transformer) const;

    bool LineIntersect(const Vector3& start, const Vector3& end, float* resultT = nullptr, int* normalIndex = nullptr, int* normalSign = nullptr) const;

    void Draw(void) const;

    static AABB Combined(const AABB& lhs, const AABB& rhs);

  private:
    Vector3 mMin;
    Vector3 mMax;
  };

}