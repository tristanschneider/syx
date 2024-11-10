#include "Precompile.h"
#include "CppUnitTest.h"

#include "generics/RateLimiter.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Test {
  constexpr uint64_t ONE_BYTE = 5;
  constexpr uint64_t TWO_BYTES = 256;
  constexpr uint64_t FOUR_BYTES = static_cast<uint32_t>(std::numeric_limits<uint16_t>::max()) + 1;
  constexpr uint64_t EIGHT_BYTES = static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 1;

  static_assert(std::is_same_v<uint8_t, decltype(gnx::impl::smallestTypeFor<ONE_BYTE>())>);
  static_assert(std::is_same_v<uint16_t, decltype(gnx::impl::smallestTypeFor<TWO_BYTES>())>);
  static_assert(std::is_same_v<uint32_t, decltype(gnx::impl::smallestTypeFor<FOUR_BYTES>())>);
  static_assert(std::is_same_v<uint64_t, decltype(gnx::impl::smallestTypeFor<EIGHT_BYTES>())>);

  TEST_CLASS(RateLimiterTest) {
    template<gnx::RateLimiter Limiter>
    void testLimiter(Limiter&& limiter, uint64_t max) {
      for(int z = 0; z < 2; ++z) {
        for(uint64_t i = 0; i < max - 1; ++i) {
          Assert::IsFalse(limiter.tryUpdate());
        }
        Assert::IsTrue(limiter.tryUpdate());
      }
    }

    template<uint64_t C>
    void testLimiter() {
      auto limiter = gnx::make_rate_limiter<C>();
      static_assert(gnx::RateLimiter<decltype(limiter)>);
      testLimiter(std::move(limiter), C);
    }

    TEST_METHOD(Static) {
      testLimiter<ONE_BYTE>();
      testLimiter<TWO_BYTES>();
      testLimiter<FOUR_BYTES>();
      //Works but takes forever...
      //testLimiter<EIGHT_BYTES>();
    }

    TEST_METHOD(Dynamic) {
      static_assert(gnx::RateLimiter<gnx::DefaultRateLimiter>);
      testLimiter(gnx::DefaultRateLimiter{ 5 }, 5);
      auto limiter = gnx::make_rate_limiter<256>();
      for(int z = 0; z < 3; ++z) {
        for(int i = 0; i < 255; ++i) {
          Assert::IsFalse(limiter.tryUpdate());
        }
        Assert::IsTrue(limiter.tryUpdate());
      }
    }
  };
}