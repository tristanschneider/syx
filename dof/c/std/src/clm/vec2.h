#pragma once

struct clm_vec2 {
  union {
    struct { float x, y; };
    struct { float u, v; };
    struct { float xy[2]; };
    struct { float uv[2]; };
  };
};
typedef struct clm_vec2 clm_vec2;

inline clm_vec2 clm_vec2_ctor(float x, float y) {
  clm_vec2 result;
  result.x = x;
  result.y = y;
  return result;
}

inline clm_vec2 clm_vec2_splat(float v) {
  return clm_vec2_ctor(v, v);
}