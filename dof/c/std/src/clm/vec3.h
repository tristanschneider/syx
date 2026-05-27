#pragma once

struct clm_vec3 {
  union {
    struct { float x, y, z; };
    struct { float xyz[3]; };
  };
};
typedef struct clm_vec3 clm_vec3;

struct clm_vec3_range {
  clm_vec3* values;
  size_t count;
};
typedef struct clm_vec3_range clm_vec3_range;

struct clm_vec3_crange {
  const clm_vec3* values;
  size_t count;
};
typedef struct clm_vec3_crange clm_vec3_crange;

inline clm_vec3 clm_vec3_ctor(float x, float y, float z) {
  clm_vec3 result;
  result.x = x;
  result.y = y;
  result.z = z;
  return result;
}

inline clm_vec3 clm_vec3_splat(float v) {
  return clm_vec3_ctor(v, v, v);
}