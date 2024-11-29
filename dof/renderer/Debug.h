#pragma once

#include "GL/glew.h"
#include "glm/vec2.hpp"

struct DebugRenderData {
  GLuint fbo{};
};

struct BoundBuffers {
  GLint arrayBuffer,
    atomicCounter,
    copyRead,
    copyWrite,
    drawIndirect,
    dispatchIndirect,
    elementArray,
    pixelPack,
    pixelUnpack,
    shaderStorage,
    transformFeedback,
    uniformBuffer,
    renderBuffer,
    sampler,
    vertexArray;
};

struct BoundProgram {
  GLint program;
};

struct Debug {
  static DebugRenderData init();
  static void pictureInPicture(const DebugRenderData& data, const glm::vec2& min, const glm::vec2& max, GLuint texture);
  static void readTransformFeedbackBuffer(GLuint buffer, std::vector<uint8_t>& result);
  static void readArrayBuffer(GLuint buffer, std::vector<uint8_t>& result);
  static void readBuffer(GLenum type, GLuint buffer, std::vector<uint8_t>& result);
  static BoundBuffers getBoundBuffers();
  static BoundProgram getBoundProgram();
};