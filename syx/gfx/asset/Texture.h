#pragma once
#include "asset/Asset.h"

class TextureLoader;

class Texture : public BufferAsset {
public:
  struct Binder {
    Binder(const Texture& tex, int slot);
    ~Binder();

    int mSlot;
  };

  Texture(AssetInfo&& info);

  //Load texture from file and upload to gpu
  void loadGpu();
  void unloadGpu();

  GLuint mTexture;
  size_t mWidth;
  size_t mHeight;
};