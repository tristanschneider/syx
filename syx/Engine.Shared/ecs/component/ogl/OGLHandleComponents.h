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

struct ShaderProgramHandleOGLComponent {
  GLHandle mProgram{};
  std::unordered_map<std::string, GLHandle> mUniforms;
};