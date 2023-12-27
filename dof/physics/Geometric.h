#pragma once

#include "glm/glm.hpp"

namespace Geo {
  constexpr float EPSILON = 0.001f;

  struct BodyMass {
    float inverseMass{};
    float inverseInertia{};
  };

  inline bool near(float a, float b, float epsilon = EPSILON) {
    return std::abs(a - b) <= epsilon;
  }

  inline bool near(const glm::vec2& a, const glm::vec2& b, float epsilon = EPSILON) {
    return near(a.x, b.x, epsilon) && near(a.y, b.y, epsilon);
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