#pragma once

#include <math.h>

struct clm_vec4 {
  union {
    struct { float i, j, k; };
    struct { float r, g, b, a; };
    struct { float x, y, z, w; };
    struct { float ijkw[4]; };
    struct { float rgba[4]; };
    struct { float xyzw[4]; };
  };
};
typedef struct clm_vec4 clm_vec4;

//Perform an element-wise expression on a vec4:
//clm_vec4_expr(i, v, a[i] * b[i] + c[i] - d[i]);
//Performs the expression on each element of abcd and assigns the result to v
#define clm_vec4_expr(it, dst, expr) for(int it = 0; it < 4; ++it) { dst.xyzw[it] = expr; }
#define clm_vec4_expri(dst, expr) clm_vec4_expr(i,dst, expr)

inline clm_vec4 clm_vec4_ctor(float x, float y, float z, float w) {
  clm_vec4 result;
  result.x = x;
  result.y = y;
  result.z = z;
  result.w = w;
  return result;
}

inline clm_vec4 clm_vec4_ptr(const float* v) {
  return clm_vec4_ctor(v[0], v[1], v[2], v[3]);
}

inline clm_vec4 clm_vec4_splat(float v) {
  return clm_vec4_ctor(v, v, v, v);
}

inline clm_vec4 clm_vec4_x() {
  return clm_vec4_ctor(1, 0, 0, 0);
}

inline clm_vec4 clm_vec4_y() {
  return clm_vec4_ctor(0, 1, 0, 0);
}

inline clm_vec4 clm_vec4_z() {
  return clm_vec4_ctor(0, 0, 1, 0);
}

inline clm_vec4 clm_vec4_w() {
  return clm_vec4_ctor(0, 0, 0, 1);
}

inline clm_vec4 clm_vec4_zero() {
  return clm_vec4_splat(0.f);
}

inline clm_vec4 clm_vec4_add(const clm_vec4* a, const clm_vec4* b) {
  return clm_vec4_ctor(a->x + b->x, a->y + b->y, a->z + b->z, a->w + b->w);
}

inline clm_vec4 clm_vec4_sub(const clm_vec4* a, const clm_vec4* b) {
  return clm_vec4_ctor(a->x - b->x, a->y - b->y, a->z - b->z, a->w - b->w);
}

inline clm_vec4 clm_vec4_mul(const clm_vec4* a, const clm_vec4* b) {
  return clm_vec4_ctor(a->x * b->x, a->y * b->y, a->z * b->z, a->w * b->w);
}

inline clm_vec4 clm_vec4_scale(const clm_vec4* v, float s) {
  return clm_vec4_ctor(v->x*s, v->y*s, v->z*s, v->w*s);
}

inline float clm_vec4_dot(const clm_vec4* a, const clm_vec4* b) {
  return a->x*b->x + a->y*b->y + a->z*b->z + a->w*b->w;
}

inline float clm_vec4_len2(const clm_vec4* v) {
  return clm_vec4_dot(v, v);
}

inline float clm_vec4_len(const clm_vec4* v) {
  return sqrtf(clm_vec4_len2(v));
}