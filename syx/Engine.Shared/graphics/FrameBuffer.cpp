#include "Precompile.h"
#include "graphics/FrameBuffer.h"

#include <gl/glew.h>

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

void FrameBuffer::bind() const {
  glBindFramebuffer(GL_FRAMEBUFFER, mFb);
}

void FrameBuffer::unBind() const {
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void FrameBuffer::bindTexture(int slot) const {
  glActiveTexture(GL_TEXTURE0 + slot);
  glBindTexture(GL_TEXTURE_2D, mTex);
}

void FrameBuffer::unBindTexture(int slot) const {
  glActiveTexture(GL_TEXTURE0 + slot);
  glBindTexture(GL_TEXTURE_2D, 0);
}

const TextureDescription& FrameBuffer::getDescription() const {
  return mDesc;
}

void FrameBuffer::_create() {
  glGenFramebuffers(1, &mFb);
  glBindFramebuffer(GL_FRAMEBUFFER, mFb);

  glGenTextures(1, &mTex);
  glBindTexture(GL_TEXTURE_2D, mTex);
  mDesc.create();

  GLuint depth;
  glGenRenderbuffers(1, &depth);
  glBindRenderbuffer(GL_RENDERBUFFER, depth);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, mDesc.width, mDesc.height);
  //Attach the depth buffer to the bound framebuffer
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);

  //Attach the texture to the bound framebuffer
  glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mTex, 0);

  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE && "Failed to create frame buffer");

  setRenderTargets<1>();

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

void FrameBuffer::_setRenderTargets(GLHandle* targets, int count) {
  //Specify this as a buffer that can be written to
  for(int i = 0; i < count; ++i)
    targets[i] = GL_COLOR_ATTACHMENT0 + i;
  glDrawBuffers(static_cast<GLsizei>(count), targets);
}