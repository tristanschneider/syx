#pragma once

#include <concepts>

namespace gnx::IntMath {
  //Value can be negative but size is assumed to always be positive
  template<std::signed_integral I>
  constexpr I wrap(I value, I size) {
    if(!size) {
      return static_cast<I>(0);
    }
    return value >= static_cast<I>(0) ? (value % size) : ((value - value * size) % size);
  }

  template<std::unsigned_integral I>
  constexpr I wrap(I value, I size) {
    return size ? (value % size) : static_cast<I>(0);
  }
}