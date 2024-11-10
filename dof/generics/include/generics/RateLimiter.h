#pragma once

#include <cassert>
#include <concepts>

namespace gnx {
  //Mechanism to perform logic once every X ticks. 1 means every tick, using the least space possible
  //In reality a dynamic implementation hardcoded to two ints would be fine but this is more amusing
  template<class T>
  concept RateLimiter = requires(T& t) {
    { t.tryUpdate() } -> std::same_as<bool>;
  };

  namespace impl {
    template<std::integral Counter>
    bool tryUpdate(Counter& current, const Counter& max) {
      if(++current >= max) {
        current = 0;
        return true;
      }
      return false;
    }

    template<auto T>
    constexpr auto smallestTypeFor() {
      if constexpr(T <= std::numeric_limits<uint8_t>::max()) {
        return uint8_t{ T };
      }
      else if constexpr(T <= std::numeric_limits<uint16_t>::max()) {
        return uint16_t{ T };
      }
      else if constexpr(T <= std::numeric_limits<uint32_t>::max()) {
        return uint32_t{ T };
      }
      else if constexpr(T <= std::numeric_limits<uint64_t>::max()) {
        return uint64_t{ T };
      }
      else {
        static_assert(sizeof(T) == -1, "Unsupported counter size");
      }
    }

    template<class I>
    concept isIntegralReference = std::integral<std::decay_t<I>> && std::is_reference_v<I>;

    template<class T>
    concept RateLimitStorage = requires(T& t) {
      { t.getValue() } -> isIntegralReference;
      { t.getMax() } -> std::integral;
      t.getValue() = t.getMax();
      typename T::value_type;
    };

    template<class T>
    concept DynamicRateLimitStorage = requires(T& t) {
      requires RateLimitStorage<T>;
      t.setMax(uint64_t{});
    };

    template<auto Max>
    struct StaticStorageImpl {
      using value_type = decltype(Max);

      value_type& getValue() {
        return value;
      }

      static constexpr value_type getMax() {
        return Max;
      }

      value_type value{};
    };

    template<std::integral T>
    struct DynamicStorageImpl {
      using value_type = T;

      value_type& getValue() {
        return value;
      }

      void setMax(value_type v) {
        max = v;
      }

      value_type getMax() {
        return max;
      }

      value_type value{};
      value_type max{};
    };
  }

  template<impl::RateLimitStorage Storage>
  class StaticRateLimiter {
  public:
    bool tryUpdate() {
      return impl::tryUpdate(storage.getValue(), storage.getMax());
    }
  protected:
    Storage storage;
  };

  template<impl::DynamicRateLimitStorage Storage>
  class DynamicRateLimiter : public StaticRateLimiter<Storage> {
  public:
    DynamicRateLimiter() = default;
    DynamicRateLimiter(uint64_t initial) {
      setMax(initial);
    }

    void setMax(uint64_t m) {
      assert(m <= std::numeric_limits<decltype(this->storage.getMax())>::max());
      this->storage.setMax(static_cast<typename Storage::value_type>(m));
    }
  };

  template<auto Counter>
  auto make_rate_limiter() {
    static_assert(std::integral<decltype(Counter)>);
    return StaticRateLimiter<impl::StaticStorageImpl<impl::smallestTypeFor<Counter>()>>{};
  }

  template<std::integral Counter>
  auto make_rate_limiter(Counter c) {
    DynamicRateLimiter<impl::StaticStorageImpl<Counter>> result;
    result.setMax(c);
    return result;
  }

  using OneInTenRateLimit = decltype(make_rate_limiter<10>());
  using OneInHundredRateLimit = decltype(make_rate_limiter<100>());
  using DefaultRateLimiter = DynamicRateLimiter<impl::DynamicStorageImpl<uint8_t>>;
}
