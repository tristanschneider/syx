#pragma once

#include <bitset>

namespace math {
  class AxisFlags {
  public:
    static constexpr AxisFlags X() { return AxisFlags{}.addX(); }
    static constexpr AxisFlags Y() { return AxisFlags{}.addY(); }
    static constexpr AxisFlags Z() { return AxisFlags{}.addZ(); }
    static constexpr AxisFlags A() { return AxisFlags{}.addA(); }
    static constexpr AxisFlags XY() { return X().addY(); }
    static constexpr AxisFlags XYZ() { return XY().addX().addZ(); }
    static constexpr AxisFlags XYZA() { return XYZ().addA(); }

    constexpr AxisFlags addX() const { return { Storage(axes | MASK_X) }; }
    constexpr AxisFlags addY() const { return { Storage(axes | MASK_Y) }; }
    constexpr AxisFlags addZ() const { return { Storage(axes | MASK_Z) }; }
    constexpr AxisFlags addA() const { return { Storage(axes | MASK_A) }; }

    constexpr bool hasX() const { return (axes & MASK_X) != 0; };
    constexpr bool hasY() const { return (axes & MASK_Y) != 0; };
    constexpr bool hasZ() const { return (axes & MASK_Z) != 0; };
    constexpr bool hasA() const { return (axes & MASK_A) != 0; };

    constexpr AxisFlags operator|(AxisFlags rhs) { return { Storage(axes | rhs.axes) }; }
    constexpr AxisFlags operator&(AxisFlags rhs) { return { Storage(axes & rhs.axes) }; }
    constexpr explicit operator bool() const { return axes != 0; }

  private:
    using Storage = uint8_t;
    static constexpr Storage MASK_X =    0b1;
    static constexpr Storage MASK_Y =   0b10;
    static constexpr Storage MASK_Z =  0b100;
    static constexpr Storage MASK_A = 0b1000;

    constexpr AxisFlags(Storage s = {})
      : axes{ s } {
    }

    Storage axes{};
  };
}