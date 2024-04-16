#pragma once
#include <type_traits>

namespace gnx {
  template<class... T>
  struct DefaultContainerTraits {
    static constexpr size_t PAGE_SIZE = 1000;
    static constexpr float GROWTH_FACTOR = 2.0f;
    static constexpr float MAX_LOAD_FACTOR = 0.8f;
    static constexpr bool SKIP_DTOR = std::conjunction_v<std::is_trivially_destructible<T>...>;
  };
}