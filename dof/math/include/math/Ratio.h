#pragma once

#include <concepts>
#include <numeric>

namespace math {
  namespace detail {
    //constexpr equivalent to std::div
    template<std::integral Int>
    constexpr auto cdiv(Int n, Int d) {
      decltype(std::div(n, d)) result{};
      result.quot = n / d;
      result.rem = n % d;
      return result;
    }
  }

  //constexpr capable equivalent to std::div
  template<std::integral Int>
  constexpr auto cdiv(Int n, Int d) {
    return std::is_constant_evaluated() ? detail::cdiv(n, d) : std::div(n, d);
  }

  //Constexpr-capable class for ratio operations similar to std::ratio_add and similar functions
  //Intended for accumulating simulation timeslices
  template<std::integral Int = int32_t>
  struct Ratio {
    constexpr Ratio(Int n = Int(0), Int d = Int(1))
      : num{ n }
      , den{ d } {
    }

    constexpr Ratio operator+(const Ratio& rhs) const {
      const Int cd = std::gcd(den, rhs.den);
      return cd ? Ratio{
        num * (rhs.den / cd) + rhs.num * (den / cd),
        den * (rhs.den / cd)
      } : Ratio{};
    }

    constexpr Ratio operator*(const Ratio& rhs) const {
      const Int gx = std::gcd(num, rhs.den);
      const Int gy = std::gcd(rhs.num, den);
      return gx && gy ? Ratio{
        (num / gx) * (rhs.num / gy),
        (den / gy) * (rhs.den / gx)
      } : Ratio{};
    }

    //Inverse without simplification
    constexpr Ratio inverse() const {
      return Ratio{ den, num };
    }

    constexpr Ratio simplify() const {
      const Int cd = std::gcd(num, den);
      return cd ? Ratio{
        num / cd,
        den / cd
      } : *this;
    }

    constexpr Ratio operator/(const Ratio& rhs) const {
      return *this * rhs.inverse();
    }

    constexpr Ratio operator-() const {
      return { -num, den };
    }

    constexpr Ratio operator-(const Ratio& rhs) const {
      return *this + (-rhs);
    }

    //Exact equality, meaning equivalent fractions are not equal if they have different denominators
    constexpr bool operator==(const Ratio& rhs) const = default;

    //Return the maximum amount of whole numbers available in the ratio and subtract it from this.
    constexpr Int popWhole() {
      if(den) {
        const Int result = num / den;
        num -= result * den;
        return result;
      }
      return {};
    }

    Int num{};
    Int den{};
  };

  using Ratio32 = Ratio<>;
}