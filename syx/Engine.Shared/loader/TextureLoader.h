#pragma once
#include "loader/AssetLoader.h"

class TextureBMPLoader : public BufferAssetLoader {
public:
  using BufferAssetLoader::BufferAssetLoader;
  AssetLoadResult _load(Asset& asset) override;
  void postProcess(const SystemArgs& args, Asset& asset) override;

private:
  //Buffer used temporarily to transform from bgr to rgba
  std::vector<uint8_t> mTempConvert;
};