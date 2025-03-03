#pragma once

namespace gnx::func {
  struct GetX { template<class T> float operator()(const T& t) const { return t.x; } };
  struct GetY { template<class T> float operator()(const T& t) const { return t.y; } };
  struct GetZ { template<class T> float operator()(const T& t) const { return t.z; } };
  struct GetW { template<class T> float operator()(const T& t) const { return t.w; } };

  struct Cos { float operator()(float f) const { return std::cos(f); } };
  struct Sin { float operator()(float f) const { return std::sin(f); } };

  struct Identity { template<class T> const T& operator()(const T& t) const { return t; } };

  template<class T>
  struct StdGet {
    template<class V> const T& operator()(const V& v) const { return std::get<T>(v); }
    template<class V> T& operator()(V& v) const { return std::get<T>(v); }
    template<class V> T&& operator()(V&& v) const { return std::move(std::get<T>(v)); }
  };

  template<auto T>
  struct GetMember {};
  template<class C, class M, M(C::*Ptr)>
  struct GetMember<Ptr> {
    const M& operator()(const C& c) const { return c.*Ptr; }
    M& operator()(C& c) const { return c.*Ptr; }
    M&& operator()(C&& c) const { return std::move(c.*Ptr); }
  };

  template<class A, class B>
  struct FMap {
    template<class T>
    auto operator()(const T& t) const {
      return B{}(A{}(t));
    }
    template<class T>
    auto operator()(T&& t) const {
      return B{}(A{}(std::move(t)));
    }
    template<class T>
    auto& operator()(T& t) const {
      return B{}(A{}(t));
    }
  };
}