#include <sandbox/Camera.h>

#include <clm/constants.h>

sbx_Camera sbx_Camera_ctor() {
  return (sbx_Camera) {
    .transform = clm_transform25_identity(),
    .fovY = CLM_PI2F,
    .zNear = 0.1f,
    .zFar = 100.f
  };
}

clm_mat4 sbx_Camera_worldToView(const sbx_Camera* c, float aspect) {
  const clm_mat4 proj = clm_mat4_perspective(c->fovY, aspect, c->zNear, c->zFar);
  const clm_mat4 world = clm_transform25_toMatrix(&c->transform);
  const clm_mat4 projWorld = clm_mat4_mul(&world, &proj);
  return clm_mat4_inverse(&projWorld);
}