#pragma once

#include "sokol_gfx.h"
#include "glm/vec2.hpp"

namespace Blit {
  struct Pass {
    sg_pipeline pipeline;
    sg_bindings bindings;
  };
  struct Transform {
    static constexpr Transform fullScreen() {
      return Transform{
        .center = glm::vec2{ 0 },
        .size = glm::vec2{ 1 }
      };
    }

    glm::vec2 center{};
    glm::vec2 size{};
  };

  Pass createBlitTexturePass();
  void blitTexture(const Transform& transform, const sg_image& texture, Pass& pass);
};