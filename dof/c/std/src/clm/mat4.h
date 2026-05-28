#pragma once

#include <clm/vec4.h>

#include <math.h>

//Row-major 4x4 matrix
struct clm_mat4 {
  union {
    struct { float r0[4]; float r1[4]; float r2[4]; float r3[4]; };
    struct {
      float r0x, r0y, r0z, r0w,
            r1x, r1y, r1z, r1w,
            r2x, r2y, r2z, r2w,
            r3x, r3y, r3z, r3w;
    };
    struct {
      float c0x, c1x, c2x, c3x,
            c0y, c1y, c2y, c3y,
            c0z, c1z, c2z, c3z,
            c0w, c1w, c2w, c3w;
    };
    struct { float data[16]; };
    struct { float rc[4][4]; };
  };
};
typedef struct clm_mat4 clm_mat4;

inline clm_mat4 clm_mat4_ctor(float a, float b, float c, float d,
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

inline clm_mat4 clm_mat4_fromCols(const clm_vec4* c0, const clm_vec4* c1, const clm_vec4* c2, const clm_vec4* c3) {
  return clm_mat4_ctor(c0->x, c1->x, c2->x, c3->x,
                       c0->y, c1->y, c2->y, c3->y,
                       c0->z, c1->z, c2->z, c3->z,
                       c0->w, c1->w, c2->w, c3->w);
}

inline clm_mat4 clm_mat4_fromRows(const clm_vec4* r0, const clm_vec4* r1, const clm_vec4* r2, const clm_vec4* r3) {
  return clm_mat4_ctor(r0->x, r0->y, r0->z, r0->w,
                       r1->x, r1->y, r1->z, r1->w,
                       r2->x, r2->y, r2->z, r2->w,
                       r3->x, r3->y, r3->z, r3->w);
}

inline clm_vec4 clm_mat4_getCol(const clm_mat4* m, int i) {
  return clm_vec4_ctor(m->r0[i], m->r1[i], m->r2[i], m->r3[i]);
}

inline clm_mat4 clm_mat4_identity() {
  return clm_mat4_ctor(1, 0, 0, 0,
                       0, 1, 0, 0,
                       0, 0, 1, 0,
                       0, 0, 0, 1);
}

inline clm_mat4 clm_mat4_perspective(float fovY, float aspect, float zNear, float zFar) {
  const float tfov = tanf(fovY / 2.f);
  const float zDist = zFar - zNear;
  return clm_mat4_ctor(1.f / (aspect * tfov),          0,                       0,                             0,
                                           0, 1.f / tfov,                       0,                             0,
                                           0,          0, -(zFar + zNear) / zDist, -(2.f * zFar * zNear) / zDist,
                                           0,          0,                    -1.f,                             0);
}

inline clm_mat4 clm_mat4_transpose(const clm_mat4* m) {
  return clm_mat4_ctor(m->r0[0], m->r1[0], m->r2[0], m->r3[0],
                       m->r0[1], m->r1[1], m->r2[1], m->r3[1],
                       m->r0[2], m->r1[2], m->r2[2], m->r3[2],
                       m->r0[3], m->r1[3], m->r2[3], m->r3[3]);
}

inline clm_mat4 clm_mat4_mul(const clm_mat4* l, const clm_mat4* r) {
  return clm_mat4_ctor(
    l->r0[0]*r->r0[0] + l->r0[1]*r->r1[0] + l->r0[2]*r->r2[0] + l->r0[3]*r->r3[0], l->r0[0]*r->r0[1] + l->r0[1]*r->r1[1] + l->r0[2]*r->r2[1] + l->r0[3]*r->r3[1], l->r0[0]*r->r0[2] + l->r0[1]*r->r1[2] + l->r0[2]*r->r2[2] + l->r0[3]*r->r3[2], l->r0[0]*r->r0[3] + l->r0[1]*r->r1[3] + l->r0[2]*r->r2[3] + l->r0[3]*r->r3[3],
    l->r1[0]*r->r0[0] + l->r1[1]*r->r1[0] + l->r1[2]*r->r2[0] + l->r1[3]*r->r3[0], l->r1[0]*r->r0[1] + l->r1[1]*r->r1[1] + l->r1[2]*r->r2[1] + l->r1[3]*r->r3[1], l->r1[0]*r->r0[2] + l->r1[1]*r->r1[2] + l->r1[2]*r->r2[2] + l->r1[3]*r->r3[2], l->r1[0]*r->r0[3] + l->r1[1]*r->r1[3] + l->r1[2]*r->r2[3] + l->r1[3]*r->r3[3],
    l->r2[0]*r->r0[0] + l->r2[1]*r->r1[0] + l->r2[2]*r->r2[0] + l->r2[3]*r->r3[0], l->r2[0]*r->r0[1] + l->r2[1]*r->r1[1] + l->r2[2]*r->r2[1] + l->r2[3]*r->r3[1], l->r2[0]*r->r0[2] + l->r2[1]*r->r1[2] + l->r2[2]*r->r2[2] + l->r2[3]*r->r3[2], l->r2[0]*r->r0[3] + l->r2[1]*r->r1[3] + l->r2[2]*r->r2[3] + l->r2[3]*r->r3[3],
    l->r3[0]*r->r0[0] + l->r3[1]*r->r1[0] + l->r3[2]*r->r2[0] + l->r3[3]*r->r3[0], l->r3[0]*r->r0[1] + l->r3[1]*r->r1[1] + l->r3[2]*r->r2[1] + l->r3[3]*r->r3[1], l->r3[0]*r->r0[2] + l->r3[1]*r->r1[2] + l->r3[2]*r->r2[2] + l->r3[3]*r->r3[2], l->r3[0]*r->r0[3] + l->r3[1]*r->r1[3] + l->r3[2]*r->r2[3] + l->r3[3]*r->r3[3]
  );
}

inline clm_mat4 clm_mat4_scale(const clm_mat4* m, float s) {
  clm_mat4 result;
  for(int i = 0; i < 16; ++i) {
    result.data[i] = m->data[i]*s;
  }
  return result;
}

//See glm func_matrix
inline clm_mat4 clm_mat4_inverse(const clm_mat4* m) {
  const float Coef00 = m->rc[2][2] * m->rc[3][3] - m->rc[2][3] * m->rc[3][2];
  const float Coef02 = m->rc[2][1] * m->rc[3][3] - m->rc[2][3] * m->rc[3][1];
  const float Coef03 = m->rc[2][1] * m->rc[3][2] - m->rc[2][2] * m->rc[3][1];

  const float Coef04 = m->rc[1][2] * m->rc[3][3] - m->rc[1][3] * m->rc[3][2];
  const float Coef06 = m->rc[1][1] * m->rc[3][3] - m->rc[1][3] * m->rc[3][1];
  const float Coef07 = m->rc[1][1] * m->rc[3][2] - m->rc[1][2] * m->rc[3][1];

  const float Coef08 = m->rc[1][2] * m->rc[2][3] - m->rc[1][3] * m->rc[2][2];
  const float Coef10 = m->rc[1][1] * m->rc[2][3] - m->rc[1][3] * m->rc[2][1];
  const float Coef11 = m->rc[1][1] * m->rc[2][2] - m->rc[1][2] * m->rc[2][1];

  const float Coef12 = m->rc[0][2] * m->rc[3][3] - m->rc[0][3] * m->rc[3][2];
  const float Coef14 = m->rc[0][1] * m->rc[3][3] - m->rc[0][3] * m->rc[3][1];
  const float Coef15 = m->rc[0][1] * m->rc[3][2] - m->rc[0][2] * m->rc[3][1];

  const float Coef16 = m->rc[0][2] * m->rc[2][3] - m->rc[0][3] * m->rc[2][2];
  const float Coef18 = m->rc[0][1] * m->rc[2][3] - m->rc[0][3] * m->rc[2][1];
  const float Coef19 = m->rc[0][1] * m->rc[2][2] - m->rc[0][2] * m->rc[2][1];

  const float Coef20 = m->rc[0][2] * m->rc[1][3] - m->rc[0][3] * m->rc[1][2];
  const float Coef22 = m->rc[0][1] * m->rc[1][3] - m->rc[0][3] * m->rc[1][1];
  const float Coef23 = m->rc[0][1] * m->rc[1][2] - m->rc[0][2] * m->rc[1][1];

  const float Fac0[4] = { Coef00, Coef00, Coef02, Coef03 };
  const float Fac1[4] = { Coef04, Coef04, Coef06, Coef07 };
  const float Fac2[4] = { Coef08, Coef08, Coef10, Coef11 };
  const float Fac3[4] = { Coef12, Coef12, Coef14, Coef15 };
  const float Fac4[4] = { Coef16, Coef16, Coef18, Coef19 };
  const float Fac5[4] = { Coef20, Coef20, Coef22, Coef23 };

  const float Vec0[4] = { m->rc[0][1], m->rc[0][0], m->rc[0][0], m->rc[0][0] };
  const float Vec1[4] = { m->rc[1][1], m->rc[1][0], m->rc[1][0], m->rc[1][0] };
  const float Vec2[4] = { m->rc[2][1], m->rc[2][0], m->rc[2][0], m->rc[2][0] };
  const float Vec3[4] = { m->rc[3][1], m->rc[3][0], m->rc[3][0], m->rc[3][0] };

  clm_vec4 Inv0, Inv1, Inv2, Inv3;
  clm_vec4_expri(Inv0, Vec1[i] * Fac0[i] - Vec2[i] * Fac1[i] + Vec3[i] * Fac2[i]);
  clm_vec4_expri(Inv1, Vec0[i] * Fac0[i] - Vec2[i] * Fac3[i] + Vec3[i] * Fac4[i]);
  clm_vec4_expri(Inv2, Vec0[i] * Fac1[i] - Vec1[i] * Fac3[i] + Vec3[i] * Fac5[i]);
  clm_vec4_expri(Inv3, Vec0[i] * Fac2[i] - Vec1[i] * Fac4[i] + Vec2[i] * Fac5[i]);

  const clm_vec4 SignA = clm_vec4_ctor(+1, -1, +1, -1);
  const clm_vec4 SignB = clm_vec4_ctor(-1, +1, -1, +1);
  const clm_vec4 C0 = clm_vec4_mul(&Inv0, &SignA);
  const clm_vec4 C1 = clm_vec4_mul(&Inv1, &SignB);
  const clm_vec4 C2 = clm_vec4_mul(&Inv2, &SignA);
  const clm_vec4 C3 = clm_vec4_mul(&Inv3, &SignB);
  const clm_mat4 Inverse = clm_mat4_fromCols(&C0, &C1, &C2, &C3);

  const clm_vec4 Row0 = clm_vec4_ptr(Inverse.r0);
  const clm_vec4 Col0 = clm_mat4_getCol(m, 0);
  const clm_vec4 Dot0 = clm_vec4_mul(&Row0, &Col0);
  const float Dot1 = (Dot0.x + Dot0.y) + (Dot0.z + Dot0.w);

  const float OneOverDeterminant = 1.f / Dot1;

  return clm_mat4_scale(&Inverse, OneOverDeterminant);
}

inline clm_vec4 clm_mat4_mul4(const clm_mat4* m, const clm_vec4* v) {
  return clm_vec4_ctor(
    m->r0x*v->x + m->r0y*v->y + m->r0z*v->z + m->r0w*v->w,
    m->r1x*v->x + m->r1y*v->y + m->r1z*v->z + m->r1w*v->w,
    m->r2x*v->x + m->r2y*v->y + m->r2z*v->z + m->r2w*v->w,
    m->r3x*v->x + m->r3y*v->y + m->r3z*v->z + m->r3w*v->w
  );
}