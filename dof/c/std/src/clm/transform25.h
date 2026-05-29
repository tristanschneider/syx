#pragma once

#include <clm/vec2.h>
#include <clm/vec3.h>
#include <clm/mat4.h>

//2.5D transform where position has a z component but rotation and scale are 2d
struct clm_transform25 {
  clm_vec3 pos;
  //Basis vector for rotation matrix meaning [cos(x), sin(x)]
  clm_vec2 rot;
  clm_vec2 scale;
};
typedef struct clm_transform25 clm_transform25;

inline clm_transform25 clm_transform25_identity() {
  clm_transform25 result;
  result.pos = clm_vec3_splat(0);
  //cos,sin of 0 degrees
  result.rot = clm_vec2_ctor(1, 0);
  result.scale = clm_vec2_splat(1.f);
  return result;
}

inline clm_mat4 clm_transform25_toMatrix(const clm_transform25* t) {
  const clm_vec3 scale = clm_vec3_ctor(t->scale.x, t->scale.y, 1.f);
  return clm_mat4_transform_build25(&t->pos, &t->rot, &scale);
}