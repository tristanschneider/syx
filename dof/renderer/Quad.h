#pragma once

#include "sokol_gfx.h"

struct QuadUniforms {
  sg_bindings bindings{ 0 };
};

struct QuadPass {
  QuadUniforms mQuadUniforms;
  //Indicates if mesh comes from SharedMeshRow (true) or MeshRow (false)
  bool sharedMesh{ true };
};
