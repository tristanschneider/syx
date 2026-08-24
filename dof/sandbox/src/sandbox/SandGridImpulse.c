#include <sandbox/SandGridImpulse.h>

clm_vec2 sbx_doRadialImpulse(const clm_vec2* pos, void* data) {
  sbx_SandGridImpulse_Radial* rad = data;
  clm_vec2 result = clm_vec2_sub(pos, &rad->center);
  result.y = -result.y;
  const float dist = clm_vec2_len(&result);
  if(dist <= 0 ){
    return clm_vec2_zero();
  }
  const float denom = (rad->radius - dist) / dist;
  return denom <= 0 ? clm_vec2_zero() : clm_vec2_scale(&result, rad->scalar / denom);
}

sbx_SandGridImpulseIterator sbx_SandGridImpulse_createRadial(sbx_SandGridImpulse_Radial* r) {
  return (sbx_SandGridImpulseIterator){ &sbx_doRadialImpulse, r };
}