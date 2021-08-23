#pragma once

namespace Syx {
  //All operations safe against division by zero
  struct Vector2 {
    Vector2(float inX, float inY): x(inX), y(inY) {}
    Vector2(float splat = 0.f): x(splat), y(splat) {}

    Vector2 operator+(const Vector2& rhs) const;
    Vector2 operator-(const Vector2& rhs) const;
    Vector2 operator*(float scalar) const;
    Vector2 operator/(float denom) const;
    bool operator==(const Vector2& rhs) const;
    bool operator!=(const Vector2& rhs) const;
    Vector2 operator-(void) const;
    float& operator[](size_t index);
    float operator[](size_t index) const;

    bool equal(const Vector2& rhs, float epsilon);

    Vector2& operator+=(const Vector2& rhs);
    Vector2& operator-=(const Vector2& rhs);
    Vector2& operator*=(float scalar);
    Vector2& operator/=(float denom);

    float dot(const Vector2& rhs) const;
    float cross(const Vector2& rhs) const;
    Vector2 proj(const Vector2& onto) const;
    float projScalar(const Vector2& onto) const;
    Vector2 scale(const Vector2& scalar) const;

    float length(void) const;
    float length2(void) const;
    float dist(const Vector2& to) const;
    float dist2(const Vector2& to) const;

    float normalize(void);
    Vector2 normalized(void) const;

    Vector2 lerp(const Vector2& to, float t) const;
    //Assumes this and to are both normalized
    Vector2 slerp(const Vector2& to, float t) const;
    Vector2 rotate(float ccwRadians) const;

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