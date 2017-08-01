#include "Precompile.h"
#include "BufferAttribs.h"

BufferAttribs::Binder::Binder(const BufferAttribs& attribs)
  : mAttribs(attribs) {
  mAttribs.bind();
}

BufferAttribs::Binder::~Binder() {
  mAttribs.unbind();
}

void BufferAttribs::addAttrib(int size, int type, int stride) {
  Attrib a;
  a.mSize = size;
  a.mType = type;
  a.mStride = stride;
  mAttribs.push_back(a);
}

void BufferAttribs::bind() const {
  for(size_t i = 0; i < mAttribs.size(); ++i) {
    //Enable this attribute
    glEnableVertexAttribArray(i);
    //Define this attribute's details
    const Attrib& a = mAttribs[i];
    glVertexAttribPointer(i, a.mSize, a.mType, GL_FALSE, a.mStride, nullptr);
  }
}

void BufferAttribs::unbind() const {
  for(size_t i = 0; i < mAttribs.size(); ++i) {
    glDisableVertexAttribArray(i);
  }
}
