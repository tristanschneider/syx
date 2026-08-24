#pragma once

#include <clm/vec2.h>

struct sbx_SandGridImpulseIterator {
  clm_vec2(*computeImpulse)(const clm_vec2* pos, void* data);
  void* data;
};
typedef struct sbx_SandGridImpulseIterator sbx_SandGridImpulseIterator;
