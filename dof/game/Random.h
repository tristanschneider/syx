#pragma once

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
};

namespace Random {
  std::unique_ptr<IRandom> twister();
}