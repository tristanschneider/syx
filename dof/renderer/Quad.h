#pragma once

#include "sokol_gfx.h"

struct QuadUniforms {
  sg_bindings bindings;
};

struct QuadPass {
  size_t mLastCount{};
  QuadUniforms mQuadUniforms;
};
