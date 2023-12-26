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
}