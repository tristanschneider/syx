#include "Precompile.h"
#include "CppUnitTest.h"

#include "generics/IntMath.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Test {
  static_assert(gnx::IntMath::wrap(-5, 2) == 1);
  static_assert(gnx::IntMath::wrap(-4, 2) == 0);
  static_assert(gnx::IntMath::wrap(-3, 2) == 1);
  static_assert(gnx::IntMath::wrap(-2, 2) == 0);
  static_assert(gnx::IntMath::wrap(-1, 2) == 1);
  static_assert(gnx::IntMath::wrap(0, 2) == 0);
  static_assert(gnx::IntMath::wrap(1, 2) == 1);
  static_assert(gnx::IntMath::wrap(2, 2) == 0);
  static_assert(gnx::IntMath::wrap(3, 2) == 1);
  static_assert(gnx::IntMath::wrap(4, 2) == 0);
  static_assert(gnx::IntMath::wrap(5, 2) == 1);

  static_assert(gnx::IntMath::wrap(0, 0) == 0);

  static_assert(gnx::IntMath::wrap(-11, 10) == 9);
  static_assert(gnx::IntMath::wrap(-10, 10) == 0);
  static_assert(gnx::IntMath::wrap(-1, 10) == 9);
  static_assert(gnx::IntMath::wrap(0, 10) == 0);
  static_assert(gnx::IntMath::wrap(1, 10) == 1);
  static_assert(gnx::IntMath::wrap(10, 10) == 0);
  static_assert(gnx::IntMath::wrap(11, 10) == 1);

  static_assert(gnx::IntMath::wrap(0u, 0u) == 0u);

  static_assert(gnx::IntMath::wrap(0u, 2u) == 0u);
  static_assert(gnx::IntMath::wrap(1u, 2u) == 1u);
  static_assert(gnx::IntMath::wrap(2u, 2u) == 0u);
  static_assert(gnx::IntMath::wrap(3u, 2u) == 1u);
  static_assert(gnx::IntMath::wrap(4u, 2u) == 0u);
  static_assert(gnx::IntMath::wrap(5u, 2u) == 1u);

  static_assert(gnx::IntMath::wrap(0u, 10u) == 0u);
  static_assert(gnx::IntMath::wrap(1u, 10u) == 1u);
  static_assert(gnx::IntMath::wrap(10u, 10u) == 0u);
  static_assert(gnx::IntMath::wrap(11u, 10u) == 1u);
}