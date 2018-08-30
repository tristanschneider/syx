#include "Precompile.h"
#include "graphics/FrameBuffer.h"

#include <gl/glew.h>

void texParamIMinMag(GLint param) {
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, param);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, param);
}

TextureDescription::TextureDescription(int _width, int _height, TextureFormat _format, TextureSampleMode _sampler)
  : width(_width)
  , height(_height)
  , format(_format)
  , sampler(_sampler) {
}

void TextureDescription::create(void* texture) const {
  switch(format) {
    case TextureFormat::RGBA8: glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, texture); break;
  }

  switch(sampler) {
    case TextureSampleMode::Nearest:
      texParamIMinMag(GL_NEAREST);
      break;
  }
}


FrameBuffer::FrameBuffer(const TextureDescription& desc)
  : mDesc(desc) {
  _create();
}

FrameBuffer::~FrameBuffer() {
  _destroy();
}

FrameBuffer::FrameBuffer(const FrameBuffer& fb)
  : FrameBuffer(fb.mDesc) {
}

FrameBuffer::FrameBuffer(FrameBuffer&& fb)
  : mDesc(fb.mDesc)
  , mFb(fb.mFb)
  , mTex(fb.mTex) 
  , mDepth(fb.mDepth) {
  fb._clear();
}

FrameBuffer& FrameBuffer::operator=(const FrameBuffer& fb) {
  _destroy();
  mDesc = fb.mDesc;
  _create();
  return *this;
}

FrameBuffer& FrameBuffer::operator=(FrameBuffer&& fb) {
  _destroy();
  mDesc = fb.mDesc;
  mFb = fb.mFb;
  mTex = fb.mTex;
  mDepth = fb.mDepth;
  fb._clear();
  return *this;
}

void FrameBuffer::bind() {
  glBindFramebuffer(GL_FRAMEBUFFER, mFb);
}

void FrameBuffer::unBind() {
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void FrameBuffer::_create() {
  glGenFramebuffers(1, &mFb);
  glBindFramebuffer(GL_FRAMEBUFFER, mFb);

  GLuint texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  mDesc.create();

  GLuint depth;
  glGenRenderbuffers(1, &depth);
  glBindRenderbuffer(GL_RENDERBUFFER, depth);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, mDesc.width, mDesc.height);
  //Attach the depth buffer to the bound framebuffer
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);

  //Attach the texture to the bound framebuffer
  glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture, 0);

  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE && "Failed to create frame buffer");

  //Specify this as a buffer that can be written to
  std::array<GLenum, 1> buffers = { GL_COLOR_ATTACHMENT0 };
  glDrawBuffers(buffers.size(), buffers.data());

  unBind();
}

void FrameBuffer::_destroy() {
  if(mFb) {
    glDeleteTextures(1, &mTex);
    glDeleteRenderbuffers(1, &mDepth);
    glDeleteFramebuffers(1, &mFb);
  }
  _clear();
}

void FrameBuffer::_clear() {
  mFb = mTex = mDepth = 0;
}