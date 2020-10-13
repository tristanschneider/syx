#pragma once

struct UniqueID {
  static constexpr UniqueID invalid() {
    return {};
  }

  static UniqueID random() {
    union Buff {
      int32_t ints[2];
      uint64_t result;
    } buff;
    buff.ints[0] = rand();
    buff.ints[1] = rand();
    return { buff.result };
  }

  constexpr UniqueID()
    : mRaw(0) {
  }

  constexpr UniqueID(uint64_t id)
    : mRaw(id) {
  }

  constexpr bool operator==(const UniqueID& rhs) const {
    return mRaw == rhs.mRaw;
  }

  constexpr bool operator!=(const UniqueID& rhs) const {
    return !(*this == rhs);
  }

  constexpr bool operator<(const UniqueID& rhs) const {
    return mRaw < rhs.mRaw;
  }

  constexpr operator bool() const {
    return *this != invalid();
  }

  uint64_t mRaw = 0;
};

namespace std {
  template<>
  struct hash<UniqueID> {
    size_t operator()(const UniqueID& id) const noexcept {
      return std::hash<decltype(id.mRaw)>()(id.mRaw);
    }
  };
}