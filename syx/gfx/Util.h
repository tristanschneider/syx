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

template<typename T, typename Lock = std::mutex>
class Guarded {
public:
  Guarded(T& obj, Lock& lock)
    : mObj(&obj)
    , mLock(&lock) {
    mLock->lock();
  }

  ~Guarded() {
    _unlock();
  }

  Guarded(const Guarded<T, Lock>&) = delete;

  Guarded(Guarded<T, Lock>&& g) {
    mLock = g.mLock;
    mObj = g.mObj;
    g._clear();
  }

  Guarded<T, Lock>& operator=(Guarded<T, Lock>&& g) {
    _unlock();
    mLock = g.mLock;
    mObj = g.mObj;
    g._clear();
  }

  Guarded<T, Lock>& operator=(const Guarded<T, Lock>&) = delete;

  T& get() {
    return *mObj;
  }

  const T& get() const {
    return *mObj;
  }

  operator T&() {
    return *mObj;
  }

  operator const T&() const {
    return *mObj;
  }

private:
  void _clear() {
    mLock = nullptr;
    mLock = nullptr;
  }

  void _unlock() {
    if(mLock)
      mLock->unlock();
  }

  Lock* mLock;
  T* mObj;
};

template<typename T>
using HandleMap = std::unordered_map<Handle, T>;
