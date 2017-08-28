#pragma once

namespace Syx {
  struct Transformer;

#ifdef SENABLED
  class SAABB {
  public:
    SAABB(SFloats min, SFloats max)
      : mMin(min)
      , mMax(max) {
    }
    SAABB() {}
    SAABB(const Vec3Vec& points) { construct(points, true); }

    void construct(const Vec3Vec& points, bool clear);

    SFloats getVolume() const;
    SFloats getInertia() const;

    SFloats getMin() const { return mMin; }
    void setMin(SFloats min) { mMin = min; }
    SFloats getMax() const { return mMax; }
    void setMax(SFloats max) { mMax = max; }
    SFloats getDiagonal() const { return SSubAll(mMax, mMin); }

  private:
    //Not to be touched directly so I can easily do a different representation if it proves to be better
    SFloats mMin;
    SFloats mMax;
  };
#endif

  class AABB {
  public:
    AABB(const Vec3& min, const Vec3& max)
      : mMin(min)
      , mMax(max) {
    }
    AABB() {}
    AABB(const Vec3Vec& points) { construct(points, true); }

    void construct(const Vec3Vec& points, bool clear);

    float getVolume(void) const;
    Vec3 getInertia(void) const;
    float getSurfaceArea(void) const;

    Vec3 getMin(void) const { return mMin; }
    void setMin(const Vec3& min) { mMin = min; }
    Vec3 getMax(void) const { return mMax; }
    void setMax(const Vec3& max) { mMax = max; }
    Vec3 getDiagonal(void) const { return mMax - mMin; }
    Vec3 getCenter(void) const { return (mMin + mMax)*0.5f; }
    bool overlapping(const AABB& other) const;
    bool isInside(const Vec3& point) const;
    bool isInside(const AABB& aabb) const;
    void pad(float padPercent);
    void move(const Vec3& amount);
    void add(const Vec3& point);
    void init(const Vec3& point);

    AABB transform(const Transformer& transformer) const;

    bool lineIntersect(const Vec3& start, const Vec3& end, float* resultT = nullptr, int* normalIndex = nullptr, int* normalSign = nullptr) const;

    void draw(void) const;

    static AABB combined(const AABB& lhs, const AABB& rhs);

  private:
    Vec3 mMin;
    Vec3 mMax;
  };

}