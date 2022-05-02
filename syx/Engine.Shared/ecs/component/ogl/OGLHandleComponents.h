#pragma once

#include "graphics/GraphicsTypes.h"

struct TextureHandleOGLComponent {
  GLHandle mTexture{};
};

struct GraphicsModelHandleOGLComponent {
  GLHandle mVertexBuffer{};
  GLHandle mIndexBuffer{};
  GLHandle mVertexArray{};
  GLHandle mIndexCount{};
};

struct ShaderProgramHandleOGLComponent {
  GLHandle mProgram{};
  std::unordered_map<std::string, GLHandle> mUniforms;
};