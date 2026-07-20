#pragma once

#include <stdint.h>

struct clm_byte4 {
  union {
    struct { uint8_t i, j, k; };
    struct { uint8_t r, g, b, a; };
    struct { uint8_t x, y, z, w; };
    struct { uint8_t ijkw[4]; };
    struct { uint8_t rgba[4]; };
    struct { uint8_t xyzw[4]; };
  };
};
typedef struct clm_byte4 clm_byte4;

inline clm_byte4 clm_byte4_ctor(uint8_t x, uint8_t y, uint8_t z, uint8_t w) {
  clm_byte4 result;
  result.x = x;
  result.y = y;
  result.z = z;
  result.w = w;
  return result;
}

inline clm_byte4 clm_byte4_splat(uint8_t v) {
  return clm_byte4_ctor(v, v, v, v);
}