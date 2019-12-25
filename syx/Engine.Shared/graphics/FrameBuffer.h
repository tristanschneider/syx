#pragma once
#include "graphics/TextureDescription.h"

class FrameBuffer {
public:
  FrameBuffer(const TextureDescription& desc);
  ~FrameBuffer();

  FrameBuffer(const FrameBuffer& fb);
  FrameBuffer(FrameBuffer&& fb);
  FrameBuffer& operator=(const FrameBuffer& fb);
  FrameBuffer& operator=(FrameBuffer&& fb);

  void bind() const;
  void unBind() const;
  void bindTexture(int slot) const;
  void unBindTexture(int slot) const;
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