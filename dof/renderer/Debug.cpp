#include "Precompile.h"
#include "Debug.h"

DebugRenderData Debug::init() {
  DebugRenderData result;
  glGenFramebuffers(1, &result.fbo);
  return result;
}

void Debug::pictureInPicture(const DebugRenderData& data, const glm::vec2& min, const glm::vec2& max, GLuint texture) {
  //Bind texture to FBO
  glBindFramebuffer(GL_READ_FRAMEBUFFER, data.fbo);
  glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

  //Figure out source size
  GLint width, height;
  glGetTextureLevelParameteriv(texture, 0, GL_TEXTURE_WIDTH, &width);
  glGetTextureLevelParameteriv(texture, 0, GL_TEXTURE_HEIGHT, &height);

  //Blit from fbo with bound texture to screen
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  glBlitFramebuffer(0, 0, width, height, (GLint)min.x, (GLint)min.y, (GLint)max.x, (GLint)max.y, GL_COLOR_BUFFER_BIT, GL_NEAREST);
}

void Debug::readTransformFeedbackBuffer(GLuint buffer, std::vector<uint8_t>& result) {
  readBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buffer, result);
}

void Debug::readArrayBuffer(GLuint buffer, std::vector<uint8_t>& result) {
  readBuffer(GL_ARRAY_BUFFER, buffer, result);
}

void Debug::readBuffer(GLenum type, GLuint buffer, std::vector<uint8_t>& result) {
  glBindBuffer(type, buffer);
  GLint size{};
  glGetBufferParameteriv(type, GL_BUFFER_SIZE, &size);
  if(auto e = glGetError()) {
    printf("Error getting buffer size %d", (int)e);
    return;
  }
  glBindBuffer(type, 0);

  result.clear();
  result.resize(size_t(size));
  glGetNamedBufferSubData(buffer, 0, size, result.data());
  if(auto e = glGetError()) {
    printf("Error reading buffer %d", (int)e);
    return;
  }
}

BoundBuffers Debug::getBoundBuffers() {
  BoundBuffers result;
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &result.arrayBuffer);
  glGetIntegerv(GL_ATOMIC_COUNTER_BUFFER_BINDING, &result.atomicCounter);
  glGetIntegerv(GL_COPY_READ_BUFFER_BINDING, &result.copyRead);
  glGetIntegerv(GL_COPY_WRITE_BUFFER_BINDING, &result.copyWrite);
  glGetIntegerv(GL_DRAW_INDIRECT_BUFFER_BINDING, &result.drawIndirect);
  glGetIntegerv(GL_DISPATCH_INDIRECT_BUFFER_BINDING, &result.dispatchIndirect);
  glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &result.elementArray);
  glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &result.pixelPack);
  glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &result.pixelUnpack);
  glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING, &result.shaderStorage);
  glGetIntegerv(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, &result.transformFeedback);
  glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &result.uniformBuffer);
  glGetIntegerv(GL_RENDERBUFFER_BINDING, &result.renderBuffer);
  glGetIntegerv(GL_SAMPLER_BINDING, &result.sampler);
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &result.vertexArray);
  return result;
}

BoundProgram Debug::getBoundProgram() {
  BoundProgram result;
  glGetIntegerv(GL_CURRENT_PROGRAM, &result.program);
  return result;
}
