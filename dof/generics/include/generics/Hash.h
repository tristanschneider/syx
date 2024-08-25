#pragma once

namespace gnx::Hash {
  template<class First, class... Args>
  size_t combine(const First& first, const Args&... rest) {
    size_t firstHash = std::hash<First>{}(first);
    if constexpr(sizeof...(rest)) {
      return firstHash ^ (combine(rest...) << 1);
    }
    else {
      return firstHash;
    }
  }
}