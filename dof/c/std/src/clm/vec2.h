#pragma once

#include <math.h>
#include <stdbool.h>

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

inline clm_vec2 clm_vec2_zero() {
  return clm_vec2_ctor(0, 0);
}

inline clm_vec2 clm_vec2_splat(float v) {
  return clm_vec2_ctor(v, v);
}

//Vector rotated around z by this amount in radians
inline clm_vec2 clm_vec2_rad(float rad) {
  return clm_vec2_ctor(cosf(rad), sinf(rad));
}

//Inverse of clm_vec2_rad
inline float clm_vec2_getRad(const clm_vec2* v) {
  return atan2f(v->y, v->x);
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

inline clm_vec2 clm_vec2_scale(const clm_vec2* v, float s) {
  return clm_vec2_ctor(v->x * s, v->y * s);
}

inline clm_vec2 clm_vec2_add(const clm_vec2* a, const clm_vec2* b) {
  return clm_vec2_ctor(a->x + b->x, a->y + b->y);
}

inline clm_vec2 clm_vec2_sub(const clm_vec2* a, const clm_vec2* b) {
  return clm_vec2_ctor(a->x - b->x, a->y - b->y);
}

inline bool clm_vec2_isZero(const clm_vec2* v) {
  return v->x == 0 && v->y == 0;
}

inline float clm_vec2_dot(const clm_vec2* a, const clm_vec2* b) {
  return a->x * b->x + a->y * b->y;
}

inline float clm_vec2_len2(const clm_vec2* v) {
  return clm_vec2_dot(v, v);
}

inline float clm_vec2_len(const clm_vec2* v) {
  return sqrtf(clm_vec2_len2(v));
}