#pragma once

#include "glm/glm.hpp"

namespace Geo {
  constexpr float EPSILON = 0.001f;

  inline bool near(float a, float b, float epsilon = EPSILON) {
    return std::abs(a - b) <= epsilon;
  }

  inline bool near(const glm::vec2& a, const glm::vec2& b, float epsilon = EPSILON) {
    return near(a.x, b.x, epsilon) && near(a.y, b.y, epsilon);
  }
}