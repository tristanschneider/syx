#pragma once

#include <std/Compare.h>
#include <stdint.h>

struct clm_irect {
  int32_t minX, minY, maxX, maxY;
};
typedef struct clm_irect clm_irect;

inline clm_irect clm_irect_fromMinMax(int minX, int minY, int maxX, int maxY) {
  clm_irect result;
  result.minX = minX;
  result.minY = minY;
  result.maxX = maxX;
  result.maxY = maxY;
  return result;
}

inline clm_irect clm_irect_limits() {
  return clm_irect_fromMinMax(INT32_MIN, INT32_MIN, INT32_MAX, INT32_MAX);
}

inline clm_irect clm_irect_zero() {
  return clm_irect_fromMinMax(0, 0, 0, 0);
}

inline int32_t clm_irect_area(const clm_irect* rect) {
  return (rect->maxX - rect->minX) * (rect->maxY - rect->minY);
}

//Returns a rect containing the common area between the two (intersect)
//If they do not intersect it will return an unspecified rect of zero area.
inline clm_irect clm_irect_intersect(const clm_irect* a, const clm_irect* b) {
  return clm_irect_fromMinMax(
    std_max(a->minX, b->minX),
    std_max(a->minY, b->minY),
    std_min(a->maxX, b->maxX),
    std_min(a->maxY, b->maxY)
  );
}