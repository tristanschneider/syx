#pragma once

#include "graphics/GraphicsTypes.h"

struct TextureHandleOGLComponent {
  GLHandle mTexture{};
};

struct GraphicsModelHandleOGLComponent {
  GLHandle mVertexBuffer{};
  GLHandle mIndexBuffer{};
  GLHandle mVertexArray{};
};