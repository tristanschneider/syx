#pragma once

#include "Shader.h"
#include "VertexAttributes.h"

struct QuadUniforms {
  TextureSamplerUniform posX, posY, rotX, rotY, uv;
  GLuint worldToView;
  GLuint texture;
};

struct QuadVertexElement {
  glm::vec2 pos;
};

struct QuadVertexAttributes : VertexAttributes<&QuadVertexElement::pos> {};

struct QuadPass {
  size_t mLastCount{};
  QuadUniforms mQuadUniforms;
};
