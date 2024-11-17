#pragma once

namespace gnx::Hash {
  constexpr size_t combineHashes(size_t hashA, size_t hashB) {
    return hashA ^ (hashB << 1);
  }

  template<class First, class... Args>
  size_t combine(size_t seed, const First& first, const Args&... rest) {
    const size_t currentHash = combineHashes(seed, std::hash<First>{}(first));
    if constexpr(sizeof...(rest)) {
      return combine(currentHash, rest...);
    }
    else {
      return currentHash;
    }
  }

  template<class First, class... Args>
  size_t combine(const First& first, const Args&... rest) {
    const size_t firstHash = std::hash<First>{}(first);
    if constexpr(sizeof...(rest)) {
      return combineHashes(firstHash, combine(rest...));
    }
    else {
      return firstHash;
    }
  }

  constexpr size_t constHash(const std::string_view& str) {
    //djb2 hash
    size_t hash = 5381;
    for(char c : str) {
      hash = ((hash << 5) + hash) + c;
    }
    return hash;
  }
}