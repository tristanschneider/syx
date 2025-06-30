#pragma once

#include <cassert>
#include <concepts>

namespace gnx::IntMath {
  template<class T>
  struct Nonzero {
    constexpr Nonzero() = delete;
    constexpr explicit Nonzero(T t) : value{ t } { assert(t); }
    constexpr T operator*() const { return value; }

    T value;
  };

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

  template<std::unsigned_integral I>
  constexpr I wrap(I value, Nonzero<I> size) {
    return value % *size;
  }

  template<std::unsigned_integral I>
  constexpr I wrappedIncrement(I value, I size) {
    return wrap(value + static_cast<I>(1), size);
  }

  template<std::unsigned_integral I>
  constexpr I wrappedIncrement(I value, Nonzero<I> size) {
    return wrap(value + static_cast<I>(1), size);
  }

  template<std::unsigned_integral I>
  constexpr I wrappedDecrement(I value, I size) {
    if(!size) {
      return static_cast<I>(0);
    }
    return (value + (size - static_cast<I>(1))) % size;
  }

  template<std::unsigned_integral I>
  constexpr I wrappedDecrement(I value, Nonzero<I> size) {
    return (value + (*size - static_cast<I>(1))) % *size;
  }
}
