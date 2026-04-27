#pragma once

#include <stdint.h>

const uint64_t STD_FNV1A64_BASIS = 14695981039346656037ULL;
const uint64_t STD_FNV1A64_PRIME = 1099511628211ULL;
const uint32_t STD_FNV1A32_BASIS = 2166136261U;
const uint32_t STD_FNV1A32_PRIME = 16777619U;

inline uint64_t std_fnv1a64Append(uint64_t value, const uint8_t* bytes, size_t count) {
  for(size_t i = 0; i < count; ++i) {
    value ^= (uint64_t)(bytes[i]);
    value *= STD_FNV1A64_PRIME;
  }
  return value;
}


inline uint32_t std_fnv1a32Append(uint32_t value, const uint8_t* bytes, size_t count) {
  for(size_t i = 0; i < count; ++i) {
    value ^= (uint32_t)(bytes[i]);
    value *= STD_FNV1A32_PRIME;
  }
  return value;
}

inline uint64_t std_fnv1a64(const uint8_t* bytes, size_t count) {
  return std_fnv1a64Append(STD_FNV1A64_BASIS, bytes, count);
}

inline uint32_t std_fnv1a32(const uint8_t* bytes, size_t count) {
  return std_fnv1a32Append(STD_FNV1A32_BASIS, bytes, count);
}

inline uint64_t std_hash64(const uint8_t* bytes, size_t count) {
  return std_fnv1a64(bytes, count);
}

inline uint32_t std_hash32(const uint8_t* bytes, size_t count) {
  return std_fnv1a32(bytes, count);
}

inline uint64_t std_hash64Append(uint64_t value, const uint8_t* bytes, size_t count) {
  return std_fnv1a64Append(value, bytes, count);
}

inline uint64_t std_hash32Append(uint32_t value, const uint8_t* bytes, size_t count) {
  return std_fnv1a32Append(value, bytes, count);
}