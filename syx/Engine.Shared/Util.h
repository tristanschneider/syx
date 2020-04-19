#pragma once
#include "Handle.h"

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

  //Get offset from owner to member, member must be direct member of owner
  template<typename Owner, typename Member>
  size_t offsetOf(const Owner& owner, const Member& member) {
    size_t result = reinterpret_cast<const uint8_t*>(&member) - reinterpret_cast<const uint8_t*>(&owner);
    assert(result + sizeof(Member) <= sizeof(Owner) && "Member must be a direct member of owner");
    return result;
  }

  template<typename Member, typename Owner>
  Member& fromOffset(Owner& owner, size_t offset) {
    return *reinterpret_cast<Member*>(reinterpret_cast<uint8_t*>(owner) + offset);
  }

  template<typename Member, typename Owner>
  const Member& fromOffset(const Owner& owner, size_t offset) {
    return *reinterpret_cast<const Member*>(reinterpret_cast<const uint8_t*>(owner) + offset);
  }

  inline const void* offset(const void* base, size_t bytes) {
    return static_cast<const void*>(static_cast<const uint8_t*>(base) + bytes);
  }

  inline void* offset(void* base, size_t bytes) {
    return static_cast<void*>(static_cast<uint8_t*>(base) + bytes);
  }

  constexpr size_t constHash(const char* str) {
    return str && *str ? static_cast<size_t>(*str) + 33ull*constHash(str + 1) : 5381ull;
  }

  template<typename Value, typename Hasher>
  std::pair<std::string, size_t> getHashPair(const Value& value, Hasher h) {
    return { value, h(value) };
  }

  std::vector<std::string_view> split(std::string_view str, std::string_view delimiter);
  std::wstring toWide(const std::string& str);
  std::string toString(const std::wstring& str);
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
