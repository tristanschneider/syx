#pragma once

class TextureLoader;

struct Texture {
  struct Binder {
    Binder(const Texture& tex, int slot);
    ~Binder();

    int mSlot;
  };

  Texture();
  Texture(const std::string& filename);

  //Load texture from file and upload to gpu
  void loadGpu(TextureLoader& loader);
  void unloadGpu();

  std::string mFilename;
  GLuint mTexture;
  Handle mHandle;
};