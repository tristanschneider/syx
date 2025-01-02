#pragma once

#include "sokol_gfx.h"

struct QuadUniforms {
  sg_bindings bindings{ 0 };
};

struct QuadPass {
  QuadUniforms mQuadUniforms;
};
