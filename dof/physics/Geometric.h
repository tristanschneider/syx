#pragma once

#include "glm/glm.hpp"

namespace Geo {
  constexpr float EPSILON = 0.001f;
  constexpr float PI = 3.14159265359f;
  constexpr float TAU = PI*2.0f;

  struct AABB {
    glm::vec2 min{ 0 };
    glm::vec2 max{ 0 };
  };

  struct BodyMass {
    float inverseMass{};
    float inverseInertia{};
  };

  struct Range1D {
    float length() const {
      return max - min;
    }
    float min{};
    float max{};
  };

  inline bool near(float a, float b, float epsilon = EPSILON) {
    return std::abs(a - b) <= epsilon;
  }

  inline bool near(const glm::vec2& a, const glm::vec2& b, float epsilon = EPSILON) {
    return near(a.x, b.x, epsilon) && near(a.y, b.y, epsilon);
  }

  inline bool nearZero(const  glm::vec2& a, float epsilon = EPSILON) {
    return std::abs(a.x + a.y) <= epsilon;
  }

  inline float cross(const glm::vec2& a, const glm::vec2& b) {
    //[ax] x [bx] = [ax*by - ay*bx]
    //[ay]   [by]
    return a.x*b.y - a.y*b.x;
  }

  inline glm::vec2 orthogonal(const glm::vec2& v) {
    //Cross product with unit Z since everything in 2D is orthogonal to Z
    //[x] [0] [ y]
    //[y]x[0]=[-x]
    //[0] [1] [ 0]
    return { v.y, -v.x };
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

  constexpr BodyMass computeQuadMass(float w, float h, float density) {
    BodyMass result;
    result.inverseMass = w*h*density;
    result.inverseInertia = result.inverseMass*(h*h + w*w)/12.0f;
    if(result.inverseMass > 0.0f) {
      result.inverseMass = 1.0f/result.inverseMass;
      result.inverseInertia = 1.0f/result.inverseInertia;
    }
    return result;
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

  bool unitAABBLineIntersect(const glm::vec2& origin, const glm::vec2& dir, float* resultTIn, float* resultTOut);
  glm::vec2 getNormalFromUnitAABBIntersect(const glm::vec2& intersect);
}