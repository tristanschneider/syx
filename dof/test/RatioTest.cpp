#include <CppUnitTest.h>

#include <math/Ratio.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Test {
  struct Popped {
    constexpr bool operator==(const Popped&) const = default;

    int32_t p{};
    math::Ratio32 r;
  };

  static constexpr Popped pop(const math::Ratio32& v) {
    auto result = v;
    const int32_t whole = result.popWhole();
    return Popped {
      .p = whole,
      .r = result
    };
  }

  TEST_CLASS(RatioTest) {
    TEST_METHOD(CDiv) {
      constexpr std::div_t compileTime = math::cdiv(2, 4);
      const std::div_t runtime = math::cdiv(2, 4);
      Assert::AreEqual(compileTime.quot, runtime.quot);
      Assert::AreEqual(compileTime.rem, runtime.rem);
    }

    static_assert(math::Ratio32{}.popWhole() == 0);
    static_assert(pop(math::Ratio32{ 1 }) == Popped{ 1, math::Ratio32{} });
    static_assert(pop(math::Ratio32{ 2 }) == Popped{ 2, math::Ratio32{} });
    static_assert(pop(math::Ratio32{ 3, 2 }) == Popped{ 1, math::Ratio32{ 1, 2 } });
    static_assert(pop(math::Ratio32{ -3, 2 }) == Popped{ -1, math::Ratio32{ -1, 2 } });
    static_assert(math::Ratio32{ 2, 4 }.simplify() == math::Ratio32{ 1, 2});
    static_assert(math::Ratio32{ -2, 4 }.simplify() == math::Ratio32{ -1, 2});
    //Pinning result for division by zero which doesn't make much sense anyway
    static_assert(math::Ratio32{ 2, 0 }.simplify() == math::Ratio32{ 1, 0 });
    static_assert(math::Ratio32{ 1, 2 } + math::Ratio32{ 1, 2 } == math::Ratio32{ 2, 2 });
    static_assert(math::Ratio32{ 1, 2 } + math::Ratio32{ 1, 3 } == math::Ratio32{ 5, 6 });
    static_assert(math::Ratio32{ 1, 2 } + math::Ratio32{} == math::Ratio32{ 1, 2 });
    static_assert(math::Ratio32{ 2, 3 } * math::Ratio32{ 3, 4 } == math::Ratio32{ 1, 2 });
    static_assert(math::Ratio32{ 2, 3 } * math::Ratio32{} == math::Ratio32{});
    static_assert(math::Ratio32{ 2, 3 } / math::Ratio32{ 3, 4 } == math::Ratio32{ 8, 9 });
    static_assert(math::Ratio32{ 2, 3 } / math::Ratio32{ 3, 4 } == math::Ratio32{ 8, 9 });
    static_assert(math::Ratio32{ 2, 3 } / math::Ratio32{ 1 } == math::Ratio32{ 2, 3 });
    //Pinning nonsense result, main point is to ensure it doesn't divide by zero
    static_assert(math::Ratio32{ 2, 3 } / math::Ratio32{} == math::Ratio32{ 1, 0 });
    static_assert(-math::Ratio32{ 2, 3 } == math::Ratio32{ -2, 3 });
    static_assert(math::Ratio32{ 1, 2 }.inverse() == math::Ratio32{ 2, 1 });
  };
}