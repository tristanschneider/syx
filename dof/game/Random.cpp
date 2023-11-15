#include "Precompile.h"
#include "Random.h"

#include <random>

namespace Random {
  std::unique_ptr<IRandom> twister() {
    struct R : IRandom {
      void generateZeroToOne(float* buffer, size_t count) override {
        //TODO: will never return 1
        constexpr int precisionBits = 10;
        for(size_t i = 0; i < count; ++i) {
          buffer[i] = std::generate_canonical<float, precisionBits>(device);
        }
      }

      void generateInRange(int32_t* buffer, int32_t min, int32_t max, size_t count) override {
        std::uniform_int_distribution<int32_t> dist(min, max);
        for(size_t i = 0; i < count; ++i) {
          buffer[i] = dist(device);
        }
      }

      std::mt19937 device{ std::random_device{}() };
    };

    return std::make_unique<R>();
  }
}