#pragma once

#include <clm/transform25.h>

struct sbx_Camera {
  clm_transform25 transform;
  float fovY;
  float zNear;
  float zFar;
};
typedef struct sbx_Camera sbx_Camera;

sbx_Camera sbx_Camera_ctor();
clm_mat4 sbx_Camera_viewToWorld(const sbx_Camera* c, float aspect);
clm_mat4 sbx_Camera_worldToView(const sbx_Camera* c, float aspect);