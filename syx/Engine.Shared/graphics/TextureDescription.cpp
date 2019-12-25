#include "Precompile.h"
#include "graphics/TextureDescription.h"

#include<gl/glew.h>

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