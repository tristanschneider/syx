#pragma once

enum class TextureFormat : uint8_t {
  RGBA8,
};

struct TextureDescription {
  int width;
  int height;
  TextureFormat format;
};

class FrameBuffer {
public:
  FrameBuffer(const TextureDescription& desc);
  ~FrameBuffer();

  FrameBuffer(const FrameBuffer& fb);
  FrameBuffer(FrameBuffer&& fb);
  FrameBuffer& operator=(const FrameBuffer& fb);
  FrameBuffer& operator=(FrameBuffer&& fb);

  void bind();
  void unBind();

private:
  void _create();
  void _destroy();
  void _clear();

  TextureDescription mDesc;
  GLHandle mFb = 0;
};