#pragma once

enum class TextureFormat : uint8_t {
  RGBA8,
};

enum class TextureSampleMode : uint8_t {
  Nearest,
};

struct TextureDescription {
  TextureDescription(int _width, int _height, TextureFormat _format, TextureSampleMode _sampler);
  void create(void* texture = nullptr) const;
  size_t bytesPerPixel() const;
  size_t totalBytes() const;
  //Format specifying the order of pixels like GL_RGBA
  GLEnum getGLFormat() const;
  //Format specifying the data type of pixels, like GL_BYTE
  GLEnum getGLType() const;

  int width;
  int height;
  TextureFormat format;
  TextureSampleMode sampler;
};
