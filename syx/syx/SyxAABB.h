#pragma once

namespace Syx {
  struct Transformer;

#ifdef SENABLED
  class SAABB {
  public:
    SAABB(SFloats min, SFloats max): mMin(min), mMax(max) {}
    SAABB(void) {}
    SAABB(const Vec3Vec& points) { Construct(points, true); }

    void Construct(const Vec3Vec& points, bool clear);

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
    AABB(const Vec3& min, const Vec3& max): mMin(min), mMax(max) {}
    AABB(void) {}
    AABB(const Vec3Vec& points) { Construct(points, true); }

    void Construct(const Vec3Vec& points, bool clear);

    float GetVolume(void) const;
    Vec3 GetInertia(void) const;
    float GetSurfaceArea(void) const;

    Vec3 GetMin(void) const { return mMin; }
    void SetMin(const Vec3& min) { mMin = min; }
    Vec3 GetMax(void) const { return mMax; }
    void SetMax(const Vec3& max) { mMax = max; }
    Vec3 GetDiagonal(void) const { return mMax - mMin; }
    Vec3 GetCenter(void) const { return (mMin + mMax)*0.5f; }
    bool Overlapping(const AABB& other) const;
    bool IsInside(const Vec3& point) const;
    bool IsInside(const AABB& aabb) const;
    void Pad(float padPercent);
    void Move(const Vec3& amount);
    void Add(const Vec3& point);
    void Init(const Vec3& point);

    AABB Transform(const Transformer& transformer) const;

    bool LineIntersect(const Vec3& start, const Vec3& end, float* resultT = nullptr, int* normalIndex = nullptr, int* normalSign = nullptr) const;

    void Draw(void) const;

    static AABB Combined(const AABB& lhs, const AABB& rhs);

  private:
    Vec3 mMin;
    Vec3 mMax;
  };

}