#pragma once

#include "Shader.h"
#include "VertexAttributes.h"

struct QuadUniforms {
  TextureSamplerUniform posX, posY, posZ, rotX, rotY, uv, velX, velY, angVel, tint, scaleX, scaleY;
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
