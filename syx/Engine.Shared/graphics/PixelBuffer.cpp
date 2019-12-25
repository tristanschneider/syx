#include "Precompile.h"
#include "graphics/PixelBuffer.h"

#include "graphics/FrameBuffer.h"
#include<gl/glew.h>

PixelBuffer::PixelBuffer(const TextureDescription& desc, Type type)
  : mDesc(desc)
  , mType(type)
  , mPb(0) {
  _create();
}

PixelBuffer::~PixelBuffer() {
  _destroy();
}

void PixelBuffer::download(GLEnum targetAttachment) {
  assert(mType == Type::Pack && "Pack type must be used to download");
  //Specify the buffer to read
  glReadBuffer(targetAttachment);
  _downloadBoundObject();
}

void PixelBuffer::download(const FrameBuffer& fb) {
  assert(mType == Type::Pack && "Pack type must be used to download");
  fb.bind();
  _downloadBoundObject();
  fb.unBind();
}

void PixelBuffer::_downloadBoundObject() {
  glBindBuffer(GL_PIXEL_PACK_BUFFER, mPb);
  //Since pixel pack is defined, data pointer becomes a read offset, start at 0
  glReadPixels(0, 0, mDesc.width, mDesc.height, mDesc.getGLFormat(), mDesc.getGLType(), 0);
}

void PixelBuffer::mapBuffer(std::vector<uint8_t>& bytes) {
  assert(mType == Type::Pack && "Pack type must be used to map buffer");
  glBindBuffer(GL_PIXEL_PACK_BUFFER, mPb);
  bytes.resize(mDesc.totalBytes());
  if(uint8_t* result = static_cast<uint8_t*>(glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY))) {
    std::memcpy(bytes.data(), result, mDesc.totalBytes());
  }
  else {
    //Indicate failure with an empty buffer
    bytes.clear();
  }
  glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
  glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
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