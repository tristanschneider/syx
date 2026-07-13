#pragma once

#include <stdint.h>

struct clm_irange {
  int32_t min, max;
};
typedef struct clm_irange clm_irange;

inline clm_irange clm_irange_ctor(int32_t min, int32_t max) {
  clm_irange result;
  result.min = min;
  result.max = max;
  return result;
}
