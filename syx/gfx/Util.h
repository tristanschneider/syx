#pragma once

#define BITWISE_OVERLOAD(func, symbol, Type)\
  inline Type func(Type l, Type r) { return (Type)((std::underlying_type_t<Type>)l symbol (std::underlying_type_t<Type>)r); }

#define BITWISE_OVERLOAD_EQ(func, symbol, Type)\
  inline Type& func(Type& l, Type r) { l = (Type)((std::underlying_type_t<Type>)l symbol (std::underlying_type_t<Type>)r); return l; }

#define MAKE_BITWISE_ENUM(Type)\
  BITWISE_OVERLOAD(operator|, |, Type)\
  BITWISE_OVERLOAD(operator&, &, Type)\
  BITWISE_OVERLOAD(operator^, ^, Type)\
  BITWISE_OVERLOAD_EQ(operator|=, |, Type)\
  BITWISE_OVERLOAD_EQ(operator&=, &, Type)\
  BITWISE_OVERLOAD_EQ(operator^=, ^, Type)

namespace Util {
  inline void hashCombine(size_t) {
  }

  template <typename T, typename... Rest>
  inline void hashCombine(size_t& seed, const T& v, Rest... rest) {
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
    hashCombine(seed, rest...);
  }
}

template <typename T>
class GuardWrapped {
public:
  GuardWrapped(T& obj, std::mutex& mutex)
    : mObj(obj)
    , mLock(mutex) {
  }

  T& get() {
    return mObj;
  }

private:
  std::unique_lock<std::mutex> mLock;
  T& mObj;
};

template<typename T>
using HandleMap = std::unordered_map<Handle, T>;
