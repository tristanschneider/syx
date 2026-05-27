#pragma once

//Row-major 4x4 matrix
struct clm_mat4 {
  union {
    struct { float r0[4]; float r1[4]; float r2[4]; float r3[4]; };
    struct { float data[16]; };
  };
};
typedef struct clm_mat4 clm_mat4;

clm_mat4 clm_mat4_ctor(float a, float b, float c, float d,
              float e, float f, float g, float h,
              float i, float j, float k, float l,
              float m, float n, float o, float p) {
  clm_mat4 result = {
    a, b, c, d,
    e, f, g, h,
    i, j, k, l,
    m, n, o, p
  };
  return result;
}

clm_mat4 clm_mat4_identity() {
  return clm_mat4_ctor(1, 0, 0, 0,
                       0, 1, 0, 0,
                       0, 0, 1, 0,
                       0, 0, 0, 1);
}