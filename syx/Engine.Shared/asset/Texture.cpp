#include "Precompile.h"
#include "Texture.h"
#include "loader/TextureLoader.h"

//#include <gl/glew.h>
//#include "GL/Glew.h"
#include "../../src-deps/glew/include/GL/glew.h"

Texture::Binder::Binder(const Texture& tex, int slot)
  : mSlot(slot) {
  glActiveTexture(GL_TEXTURE0 + slot);
  glBindTexture(GL_TEXTURE_2D, tex.mTexture);
}

Texture::Binder::~Binder() {
  glActiveTexture(GL_TEXTURE0 + mSlot);
  glBindTexture(GL_TEXTURE_2D, 0);
}

Texture::Texture(AssetInfo&& info)
  : BufferAsset(std::move(info))
  , mTexture(0) {
}

void Texture::loadGpu() {
  if(mTexture) {
    printf("Tried to upload texture that already was");
    return;
  }

  glGenTextures(1, &mTexture);
  glBindTexture(GL_TEXTURE_2D, mTexture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, mWidth, mHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, get().data());
  //Define sampling mode, no mip maps snap to nearest
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  mState = AssetState::PostProcessed;
}

void Texture::unloadGpu() {
  if(!mTexture) {
    printf("Tried to unload texture that already was");
    return;
  }
  glDeleteTextures(1, &mTexture);
  mTexture = 0;
}
