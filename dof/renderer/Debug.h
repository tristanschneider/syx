#pragma once

#include "sokol_gfx.h"
#include "glm/vec2.hpp"

struct DebugRenderData {
  sg_image fbo;
  sg_pipeline pictureInPicture;
};

struct Debug {
  static DebugRenderData init();
  static void pictureInPicture(const DebugRenderData& data, const glm::vec2& min, const glm::vec2& max, sg_image texture);
};