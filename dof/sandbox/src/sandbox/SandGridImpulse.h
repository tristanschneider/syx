#pragma once

#include <sandbox/SandGridImpulseIterator.h>

//Apply an impulse with linear falloff from center to radius with a scalar applied after falloff
struct sbx_SandGridImpulse_Radial {
  clm_vec2 center;
  float scalar;
  float radius;
};
typedef struct sbx_SandGridImpulse_Radial sbx_SandGridImpulse_Radial;

sbx_SandGridImpulseIterator sbx_SandGridImpulse_createRadial(sbx_SandGridImpulse_Radial* r);