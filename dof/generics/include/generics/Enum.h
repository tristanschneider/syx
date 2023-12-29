#pragma once

namespace gnx {
  template<class T>
  constexpr auto enumCast(T e) {
    return static_cast<std::underlying_type_t<T>>(e);
  }
}