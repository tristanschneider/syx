#pragma once

#include <Constants.h>
#include "glm/glm.hpp"

namespace Geo {
  constexpr glm::vec3 toVec3(const glm::vec2& v) { return { v.x, v.y, 0.0f }; }
  constexpr glm::vec2 toVec2(const glm::vec3& v) { return { v.x, v.y }; }

  struct AABB {
    glm::vec2 center() const {
      return (min + max) * 0.5f;
    }

    constexpr void buildInit() {
      min = glm::vec2{ std::numeric_limits<float>::max() };
      max = glm::vec2{ std::numeric_limits<float>::lowest() };
    }

    constexpr void buildAdd(const glm::vec2& p) {
      min = glm::min(min, p);
      max = glm::max(max, p);
    }

    glm::vec2 min{ 0 };
    glm::vec2 max{ 0 };
  };

  struct LineSegment {
    glm::vec2 start{ 0 };
    glm::vec2 end{ 0 };
  };

  struct Range1D {
    float length() const {
      return max - min;
    }
    float min{};
    float max{};
  };

  constexpr float squared(float v) {
    return v*v;
  }

  //Determinant but glm already has a global function named that
  constexpr float det(const glm::vec2& col1, const glm::vec2& col2) {
    return col1.x*col2.y - col1.y*col2.x;
  }

  inline bool near(float a, float b, float epsilon = Constants::EPSILON) {
    return std::abs(a - b) <= epsilon;
  }

  inline bool near(const glm::vec2& a, const glm::vec2& b, float epsilon = Constants::EPSILON) {
    return near(a.x, b.x, epsilon) && near(a.y, b.y, epsilon);
  }

  inline bool nearZero(const glm::vec2& a, float epsilon = Constants::EPSILON) {
    return std::abs(a.x + a.y) <= epsilon;
  }

  inline bool nearZero(float a, float epsilon = Constants::EPSILON) {
    return std::abs(a) <= epsilon;
  }

  inline float cross(const glm::vec2& a, const glm::vec2& b) {
    //[ax] x [bx] = [ax*by - ay*bx]
    //[ay]   [by]
    return a.x*b.y - a.y*b.x;
  }

  inline float crossXAxis(const glm::vec2& a, float scale) {
    //[ax] x [s] = [ax*0 - ay*s]
    //[ay]   [0]
    return -a.y*scale;
  }

  inline float crossYAxis(const glm::vec2& a, float scale) {
    //[ax] x [0] = [ax*s - ay*0]
    //[ay]   [s]
    return a.x*scale;
  }

  constexpr float hasDifferentSign(float a, float b) {
    //Positive times positive is positive (same)
    //Negative times negative is positive (same)
    //Positive times negative is negative (different)
    //Count zero as different
    return a * b <= 0;
  }

  constexpr float normalized(float v) {
    return v > 0.f ? 1.f : -1.f;
  }

  constexpr float makeSameSign(float v, float sign) {
    return v * normalized(sign);
  }

  inline glm::vec2 orthogonal(const glm::vec2& v) {
    //Cross product with unit Z since everything in 2D is orthogonal to Z
    //[x] [0] [ y]
    //[y]x[0]=[-x]
    //[0] [1] [ 0]
    return { v.y, -v.x };
  }

  //v x z
  inline glm::vec2 crossZ(const glm::vec2& v) {
    return orthogonal(v);
  }

  constexpr bool between(float v, float min, float max) {
    return v >= min && v <= max;
  }

  constexpr bool between(float v, const Range1D& range) {
    return between(v, range.min, range.max);
  }

  enum class RangeOverlap : uint8_t {
    AABB,
    BBAA,
    ABAB,
    ABBA,
    BAAB,
    BABA
  };

  //Normal towards a, prioritizes preventing ranges from crossing each-other rather than finding the smallest direction to resolve overlap
  constexpr float getRangeNormal(RangeOverlap o) {
    switch(o) {
      //A is on the left
      case RangeOverlap::AABB:
      case RangeOverlap::ABAB:
        return -1.0f;
      //Shapes are contained in eachother. These could go either way, arbitrarily choose 1
      case RangeOverlap::ABBA:
      case RangeOverlap::BAAB:
        return 1.0f;
        //A is on the right
      case RangeOverlap::BBAA:
      case RangeOverlap::BABA:
        return 1.0f;
    }
    return 1.0f;
  }

  //Get the distance between them. Positive if they are not overlapping. In direction of normal
  //If they are overlapping it's the overlap amount, meaning the amount along the normal to move to no longer overlap
  constexpr float getRangeDistance(RangeOverlap o, const Range1D& a, const Range1D& b) {
    switch(o) {
      case RangeOverlap::AABB:
        return b.min - a.max;
      case RangeOverlap::ABAB:
        return -(a.max - b.min);
      case RangeOverlap::ABBA:
        return -(a.max - b.min);
      case RangeOverlap::BAAB:
        return -(b.max - a.min);
      case RangeOverlap::BBAA:
        return a.min - b.max;
      case RangeOverlap::BABA:
        return -(b.max - a.min);
    }
    return 0.0f;
  }

  constexpr RangeOverlap classifyRangeOverlap(const Range1D& a, const Range1D& b) {
    if(a.min < b.min) {
      // a- a+ b- b+
      if(a.max < b.min) {
        return RangeOverlap::AABB;
      }
      // a- b- a+ b+
      // a- b- b+ a+
      return a.max < b.max ? RangeOverlap::ABAB : RangeOverlap::ABBA;
    }
    // b- b+ a- a+
    if (a.min > b.max) {
      return RangeOverlap::BBAA;
    }
    // b- a- a+ b+
    // b- a- b+ a+
    return a.max < b.max ? RangeOverlap::BAAB : RangeOverlap::BABA;
  }

  inline glm::mat3 buildTranslate(const glm::vec2& t, float z = 1.0f) {
    glm::mat3 result;
    result[0] = glm::vec3(1, 0, 0);
    result[1] = glm::vec3(0, 1, 0);
    result[2] = glm::vec3(t.x, t.y, z);
    return result;
  }

  inline glm::mat3 buildScale(const glm::vec2& s) {
    return {
      s.x, 0.0f, 0.0f,
      0.0f, s.y, 0.0f,
      0.0f, 0.0f, 1.0f
    };
  }

  inline glm::vec2 directionFromAngle(float radians) {
    return { std::cos(radians), std::sin(radians) };
  }

  //Assumed normalized
  inline float angleFromDirection(const glm::vec2& dir) {
    return std::atan2f(dir.y, dir.x);
  }

  inline glm::mat3 buildRotate(const glm::vec2& r) {
    glm::mat3 result;
    const glm::vec2& basisX = r;
    const glm::vec2 basisY = orthogonal(basisX);
    result[0] = glm::vec3(basisX.x, basisX.y, 0.0f);
    result[1] = glm::vec3(basisY.x, basisY.y, 0.0f);
    result[2] = glm::vec3(0, 0, 1);
    return result;
  }

  inline glm::mat3 buildTransform(const glm::vec2& translate, const glm::vec2& rotate, const glm::vec2& scale) {
    //Could be done in less operations if built by hand
    return buildTranslate(translate) * buildRotate(rotate) * buildScale(scale);
  }

  //Transform vector, not point, meaning last row is ignored, as in [x, y, 0]
  inline glm::vec2 transformVector(const glm::mat3& transform, const glm::vec2& v) {
    return {
      transform[0].x*v.x + transform[1].x*v.y,
      transform[0].y*v.x + transform[1].y*v.y
    };
  }

  inline glm::vec2 transformPoint(const glm::mat3& transform, const glm::vec2& p) {
    //Could optimize out some of the zeroes from the last row
    const glm::vec3 result = transform*glm::vec3{ p.x, p.y, 1.0f };
    return { result.x, result.y };
  }

  inline glm::vec2 rotate(const glm::vec2& basisX, const glm::vec2& p) {
    //[basisX.x, -basisX.y][p.x]
    //[basisX.y, basisX.x ][p.y]
    return { basisX.x*p.x - basisX.y*p.y, basisX.y*p.x + basisX.x*p.y };
  }

  constexpr float inverseOrZero(float v) {
    return v ? 1.0f/v : 0.0f;
  }

  constexpr glm::vec2 inverseOrZero(const glm::vec2& v) {
    return { inverseOrZero(v.x), inverseOrZero(v.y) };
  }

  template<class T>
  constexpr T cabs(T v) {
    return v < static_cast<T>(0) ? -v : v;
  }

  constexpr float manhattanDistance(const glm::vec2& v) {
    return cabs(v.x) + cabs(v.y);
  }

  //Transpose of a rotation matrix from the first basis vector. Transpose is the inverse of a rotation matrix.
  inline glm::vec2 transposeRot(const glm::vec2 basisX) {
    //[basisX.x, -basisX.y] to [basisX.x , basisX.y]
    //[basisX.y, basisX.x ]    [-basisX.y, basisX.x]
    return { basisX.x, -basisX.y };
  }

  inline glm::vec2 divideOr(const glm::vec2& v, float divisor, const glm::vec2& fallback, float E = Constants::EPSILON) {
    return divisor > E ? v/divisor : fallback;
  }

  inline glm::vec2 normalizedOr(const glm::vec2& v, const glm::vec2& fallback, float E = Constants::EPSILON) {
    return divideOr(v, glm::length(v), fallback, E);
  }

  inline glm::vec2 normalizedOrAny(const glm::vec2& v, float E = Constants::EPSILON) {
    return normalizedOr(v, glm::vec2{ 1, 0 }, E);
  }

  inline glm::vec2 normalizedOrZero(const glm::vec2& v, float E = Constants::EPSILON) {
    return normalizedOr(v, glm::vec2{ 0 }, E);
  }

  //Make a value smaller towards zero without passing it
  //v is positive or negative and amount is assumed positive as the amount to reduce v towards zero
  inline float reduce(float v, float amount) {
    if(v > 0) {
      return std::max(0.0f, v - amount);
    }
    return std::min(0.0f, v + amount);
  }

  bool unitAABBLineIntersect(const glm::vec2& origin, const glm::vec2& dir, float* resultTIn, float* resultTOut);
  glm::vec2 getNormalFromUnitAABBIntersect(const glm::vec2& intersect);
}