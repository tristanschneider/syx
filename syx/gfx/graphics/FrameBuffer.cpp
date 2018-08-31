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

size_t TextureDescription::bytesPerPixel() const {
  switch(format) {
    case TextureFormat::RGBA8: return 4;
    default: return 0;
  }
}

size_t TextureDescription::totalBytes() const {
  return bytesPerPixel()*width*height;
}

GLEnum TextureDescription::getGLFormat() const {
  switch(format) {
    case TextureFormat::RGBA8: return GL_RGBA;
    default: return 0;
  }
}

GLEnum TextureDescription::getGLType() const {
  switch(format) {
    case TextureFormat::RGBA8: return GL_UNSIGNED_BYTE;
    default: return 0;
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

const TextureDescription& FrameBuffer::getDescription() const {
  return mDesc;
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

PixelBuffer::PixelBuffer(const TextureDescription& desc, Type type)
  : mDesc(desc)
  , mType(type)
  , mPb(0) {
}

void PixelBuffer::download(GLEnum targetAttachment) {
  assert(mType == Type::Pack && "Pack type must be used to download");
  //Specify the buffer to read
  glReadBuffer(targetAttachment);
  glBindBuffer(GL_PIXEL_PACK_BUFFER, mPb);
  //Since pixel pack is defined, data pointer becomes a read offset, start at 0
  glReadPixels(0, 0, mDesc.width, mDesc.height, mDesc.getGLFormat(), mDesc.getGLType(), 0);
}

void PixelBuffer::mapBuffer(std::vector<uint8_t>& bytes) {
  assert(mType == Type::Pack && "Pack type must be used to map buffer");
  glBindBuffer(GL_PIXEL_PACK_BUFFER, mPb);
  bytes.resize(mDesc.totalBytes());
  uint8_t* result = static_cast<uint8_t*>(glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY));
  std::memcpy(bytes.data(), result, mDesc.totalBytes());
}

GLEnum PixelBuffer::_glType() const {
  return mType == Type::Pack ? GL_PIXEL_PACK_BUFFER : GL_PIXEL_UNPACK_BUFFER;
}

void PixelBuffer::_bind() const {
  glBindBuffer(_glType(), mPb);
}

void PixelBuffer::_unbind() const {
  glBindBuffer(_glType(), 0);
}

void PixelBuffer::_create() {
  glGenBuffers(1, &mPb);
  glBindBuffer(_glType(), mPb);
  glBufferData(_glType(), mDesc.totalBytes(), nullptr, mType == Type::Pack ? GL_STREAM_READ : GL_STREAM_DRAW);
}

void PixelBuffer::_destroy() {
  if(mPb) {
    glDeleteBuffers(1, &mPb);
    _clear();
  }
}

void PixelBuffer::_clear() {
  mPb = 0;
}