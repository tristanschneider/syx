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

  int width;
  int height;
  TextureFormat format;
  TextureSampleMode sampler;
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
  GLHandle mTex = 0;
  GLHandle mDepth = 0;
};