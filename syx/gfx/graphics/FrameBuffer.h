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

class PixelBuffer {
public:
  enum class Type : uint8_t {
    Pack,
    Unpack
  };
  PixelBuffer(const TextureDescription& desc, Type type);
  PixelBuffer(const PixelBuffer& fb) = delete;
  PixelBuffer(PixelBuffer&& fb) = delete;
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
  const TextureDescription& getDescription() const;

  template<int Targets>
  static void setRenderTargets() {
    std::array<GLHandle, Targets> targets;
    _setRenderTargets(targets.data(), Targets);
  }

private:
  static void _setRenderTargets(GLHandle* targets, int count);

  void _create();
  void _destroy();
  void _clear();

  TextureDescription mDesc;
  GLHandle mFb = 0;
  GLHandle mTex = 0;
  GLHandle mDepth = 0;
};