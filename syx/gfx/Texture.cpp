#include "Precompile.h"
#include "Texture.h"
#include "TextureLoader.h"

Texture::Binder::Binder(const Texture& tex, int slot)
  : mSlot(slot) {
  glActiveTexture(GL_TEXTURE0 + slot);
  glBindTexture(GL_TEXTURE_2D, tex.mTexture);
}

Texture::Binder::~Binder() {
  glActiveTexture(GL_TEXTURE0 + mSlot);
  glBindTexture(GL_TEXTURE_2D, 0);
}

Texture::Texture()
  : mTexture(0) {
}

Texture::Texture(const std::string& filename)
  : mFilename(filename)
  , mTexture(0) {
}

void Texture::loadGpu(TextureLoader& loader) {
  if(mTexture) {
    printf("Tried to upload texture that already was");
    return;
  }

  const TextureLoader::Texture& tex = loader.loadBmp(mFilename);
  if(tex.mBuffer) {
    glGenTextures(1, &mTexture);
    glBindTexture(GL_TEXTURE_2D, mTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex.mWidth, tex.mHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex.mBuffer->data());
    //Define sampling mode, no mip maps snap to nearest
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  }
}

void Texture::unloadGpu() {
  if(!mTexture) {
    printf("Tried to unload texture that already was");
    return;
  }
  glDeleteTextures(1, &mTexture);
  mTexture = 0;
}
