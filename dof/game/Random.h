#pragma once

#include "glm/vec2.hpp"

struct IRandom {
  virtual ~IRandom() = default;
  virtual void generateZeroToOne(float* buffer, size_t count) = 0;
  virtual void generateInRange(int32_t* buffer, int32_t min, int32_t max, size_t count) = 0;
  //TODO: shuffle

  void generateRange(float* buffer, float min, float max, size_t count) {
    generateZeroToOne(buffer, count);
    const float magnitude = max - min;
    for(size_t i = 0; i < count; ++i) {
      buffer[i] = buffer[i]*magnitude - min;
    }
  }

  float nextFloatInRange(float min, float max) {
    float result;
    generateRange(&result, min, max, 1);
    return result;
  }

  int32_t nextIntInRange(int min, int max) {
    int result;
    generateInRange(&result, min, max, 1);
    return result;
  }

  float nextZeroToOne() {
    float result;
    generateZeroToOne(&result, 1);
    return result;
  }

  glm::vec2 nextDirection() {
    float angle{};
    generateZeroToOne(&angle, 1);
    constexpr float twoPi = 2.0f * 3.141f;
    angle *= twoPi;
    return { std::cos(angle), std::sin(angle) };
  }
};

namespace Random {
  std::unique_ptr<IRandom> twister();
}