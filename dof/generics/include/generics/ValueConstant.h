#pragma once

namespace gnx {
  // Like std::integral_constant but for whatever is provided assuming it's constexpr capable
  template<class T, T v = T{}>
  struct value_constant {
    using value_type = T;
    using type = value_constant<T, v>;

    static constexpr auto value = v;

    operator value_type() const { return value; }
    value_type operator()() const { return value; }
  };

  // Like std::integral_constant but for floats
  template<float F>
  struct float_constant : value_constant<float, F> {};
}