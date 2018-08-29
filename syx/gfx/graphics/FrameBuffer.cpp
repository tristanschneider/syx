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
  , mFb(fb.mFb) {
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
  fb._clear();
  return *this;
}

void FrameBuffer::bind() {
  //glbindthis
}

void FrameBuffer::unBind() {
  //glbind 0
}

void FrameBuffer::_create() {
  //TODO: glcreatethis
}

void FrameBuffer::_destroy() {
  if(mFb) {
    //TODO: glfreethis
  }
  _clear();
}

void FrameBuffer::_clear() {
  mFb = 0;
}