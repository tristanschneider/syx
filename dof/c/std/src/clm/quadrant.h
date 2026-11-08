#pragma once

#include <clm/vec2.h>

//Identifiers for regions in and around a quadrant
enum clm_quadrant_region {
  CLM_QUADRANT_INSIDE,
  CLM_QUADRANT_NW,
  CLM_QUADRANT_N,
  CLM_QUADRANT_NE,
  CLM_QUADRANT_E,
  CLM_QUADRANT_SE,
  CLM_QUADRANT_S,
  CLM_QUADRANT_SW,
  CLM_QUADRANT_W,
  CLM_QUADRANT_COUNT,
};

//Return the region type for when the position exceeds the epsilon in any of the directions.
//In other words, the region relative to a box with the provided half-width and half-height
inline enum clm_quadrant_region clm_quadrant_getRegion(const clm_vec2* v, float epsilon) {
  //North
  if(v->y > epsilon) {
   if(v->x > epsilon) {
     return CLM_QUADRANT_NE;
   }
   if(v->x < -epsilon) {
     return CLM_QUADRANT_NW;
   }
   return CLM_QUADRANT_N;
  }
  //South
  if(v->y < -epsilon) {
   if(v->x > epsilon) {
     return CLM_QUADRANT_SE;
   }
   if(v->x < -epsilon) {
     return CLM_QUADRANT_SW;
   }
   return CLM_QUADRANT_S;
  }
  //East and West
  if(v->x > epsilon) {
    return CLM_QUADRANT_E;
  }
  if(v->x < -epsilon) {
    return CLM_QUADRANT_W;
  }
  return CLM_QUADRANT_INSIDE;
}

inline clm_vec2 clm_quadrant_vec2FromRegion(enum clm_quadrant_region region, float halfSize) {
  switch(region) {
    case CLM_QUADRANT_INSIDE: return clm_vec2_zero();
    case CLM_QUADRANT_NW: return clm_vec2_ctor(-halfSize, halfSize);
    case CLM_QUADRANT_N: return clm_vec2_ctor(0.f, halfSize);
    case CLM_QUADRANT_NE: return clm_vec2_ctor(halfSize, halfSize);
    case CLM_QUADRANT_E: return clm_vec2_ctor(halfSize, 0.f);
    case CLM_QUADRANT_SE: return clm_vec2_ctor(0.f, -halfSize);
    case CLM_QUADRANT_S: return clm_vec2_ctor(0.f, -halfSize);
    case CLM_QUADRANT_SW: return clm_vec2_ctor(-halfSize, -halfSize);
    case CLM_QUADRANT_W: return clm_vec2_ctor(-halfSize, 0.f);
  }
  return clm_vec2_zero();
}

//Compute the index in a 2D array of the quadrant in relation to the inside of the quadrant.
inline int32_t clm_quadrant_index2D(enum clm_quadrant_region region, int32_t stride) {
  switch(region) {
    case CLM_QUADRANT_INSIDE: return 0;
    case CLM_QUADRANT_NW: return -stride - 1;
    case CLM_QUADRANT_N: return -stride;
    case CLM_QUADRANT_NE: return -stride + 1;
    case CLM_QUADRANT_E: return 1;
    case CLM_QUADRANT_SE: return stride + 1;
    case CLM_QUADRANT_S: return stride;
    case CLM_QUADRANT_SW: return stride - 1;
    case CLM_QUADRANT_W: return -1;
  }
  return 0;
}