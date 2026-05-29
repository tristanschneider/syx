#pragma once

#include <math.h>

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

//Vector rotated around z by this amount in radians
inline clm_vec2 clm_vec2_rad(float rad) {
  return clm_vec2_ctor(cosf(rad), sinf(rad));
}

//Rotate v by rot where rot is of the same form as clm_vec2_rad
inline clm_vec2 clm_vec2_rotate(const clm_vec2* v, const clm_vec2* rot) {
  //[rx,-ry]*[x]=[rx*x-ry*y]
  //[ry, rx] [y] [ry*x+rx*y]
  return clm_vec2_ctor(
    rot->x*v->x - rot->y*v->y,
    rot->y*v->x + rot->x*v->y
  );
}