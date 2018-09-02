#pragma once
#include "graphics/TextureDescription.h"

class PixelBuffer {
public:
  enum class Type : uint8_t {
    Pack,
    Unpack
  };
  PixelBuffer(const TextureDescription& desc, Type type);
  PixelBuffer(const PixelBuffer& fb) = delete;
  PixelBuffer(PixelBuffer&& fb) = delete;
  ~PixelBuffer();
  PixelBuffer& operator=(const PixelBuffer& fb) = delete;
  PixelBuffer& operator=(PixelBuffer&& fb) = delete;

  //Start asynchronous download/upload
  //Attach location of target like GL_FRONT or GL_COLOR_ATTACHMENTX
  void download(GLEnum targetAttachment);

  //Map previous asynchronous action to client space
  void mapBuffer(std::vector<uint8_t>& bytes);

private:
  GLEnum _glType() const;
  void _bind() const;
  void _unbind() const;
  void _create();
  void _destroy();
  void _clear();

  TextureDescription mDesc;
  Type mType;
  GLHandle mPb;
};