#include "Precompile.h"
#include "loader/TextureLoader.h"

#include "App.h"
#include "asset/Texture.h"
#include "system/AssetRepo.h"
#include "system/GraphicsSystem.h"

RegisterAssetLoader("bmp", TextureBMPLoader, Texture);

namespace {
  const int sDataPosOffset = 0x0A;
  const int sImageSizeOffset = 0x22;
  const int sWidthOffset = 0x12;
  const int sHeightOffset = 0x16;
  const size_t sHeaderSize = 54;
}

AssetLoadResult TextureBMPLoader::_load(Asset& asset) {
  if(mData.size() < sHeaderSize) {
    printf("Error reading header of bmp at %s\n", asset.getInfo().mUri.c_str());
    return AssetLoadResult::Fail;
  }

  if(mData[0] != 'B' || mData[1] != 'M') {
    printf("Not a bmp file at %s\n", asset.getInfo().mUri.c_str());
    return AssetLoadResult::Fail;
  }

  uint32_t dataStart = reinterpret_cast<uint32_t&>(mData[sDataPosOffset]);
  uint32_t imageSize = reinterpret_cast<uint32_t&>(mData[sImageSizeOffset]);
  uint16_t width = reinterpret_cast<uint16_t&>(mData[sWidthOffset]);
  uint16_t height = reinterpret_cast<uint16_t&>(mData[sHeightOffset]);

  //If the fields are missing, fill them in
  if(!imageSize)
    imageSize=width*height*3;
  if(!dataStart)
    dataStart = 54;

  if(mData.size() - dataStart < imageSize) {
    printf("Invalid bmp data at %s\n", asset.getInfo().mUri.c_str());
    return AssetLoadResult::Fail;
  }

  //Resize in preparation for the fourth component we'll add to each pixel
  mTempConvert.resize(width*height*4);
  for(uint16_t i = 0; i < width*height; ++i) {
    size_t bp = 4*i;
    size_t tp = 3*i + dataStart;
    //Flip bgr to rgb and add empty alpha
    mTempConvert[bp] = mData[tp + 2];
    mTempConvert[bp + 1] = mData[tp + 1];
    mTempConvert[bp + 2] = mData[tp];
    mTempConvert[bp + 3] = 0;
  }

  Texture& tex = static_cast<Texture&>(asset);
  tex.mWidth = width;
  tex.mHeight = height;
  tex.set(std::move(mTempConvert));
  return AssetLoadResult::Success;
}

void TextureBMPLoader::postProcess(App& app, Asset& asset) {
  app.getSystem<GraphicsSystem>()->dispatchToRenderThread([&asset] {
    static_cast<Texture&>(asset).loadGpu();
  });
}
