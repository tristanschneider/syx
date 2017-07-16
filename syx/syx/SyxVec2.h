#pragma once

namespace Syx {
  //All operations safe against division by zero
  struct Vector2 {
    Vector2(float inX, float inY): x(inX), y(inY) {}
    Vector2(float splat): x(splat), y(splat) {}
    //Doesn't initialize anything
    Vector2(void) {}

    Vector2 operator+(const Vector2& rhs) const;
    Vector2 operator-(const Vector2& rhs) const;
    Vector2 operator*(float scalar) const;
    Vector2 operator/(float denom) const;
    bool operator==(const Vector2& rhs) const;
    bool operator!=(const Vector2& rhs) const;
    Vector2 operator-(void) const;

    bool Equal(const Vector2& rhs, float epsilon);

    Vector2& operator+=(const Vector2& rhs);
    Vector2& operator-=(const Vector2& rhs);
    Vector2& operator*=(float scalar);
    Vector2& operator/=(float denom);

    float Dot(const Vector2& rhs) const;
    float Cross(const Vector2& rhs) const;
    Vector2 Proj(const Vector2& onto) const;
    float ProjScalar(const Vector2& onto) const;
    Vector2 Scale(const Vector2& scalar) const;

    float Length(void) const;
    float Length2(void) const;
    float Dist(const Vector2& to) const;
    float Dist2(const Vector2& to) const;

    float Normalize(void);
    Vector2 Normalized(void) const;

    Vector2 Lerp(const Vector2& to, float t) const;
    //Assumes this and to are both normalized
    Vector2 Slerp(const Vector2& to, float t) const;
    Vector2 Rotate(float ccwRadians) const;

    static const Vector2 sUnitX;
    static const Vector2 sUnitY;
    static const Vector2 sZero;
    static const Vector2 sIdentity;

    float x;
    float y;
  };

  Vector2 operator*(float lhs, const Vector2& rhs);

  typedef Vector2 Vec2;
}