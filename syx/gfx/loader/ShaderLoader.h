#pragma once
#include "loader/AssetLoader.h"

class ShaderLoader : public AssetLoader {
public:
  using AssetLoader::AssetLoader;
  virtual ~ShaderLoader();

  AssetLoadResult load(const std::string& basePath, Asset& asset) override;
  void postProcess(App& app, Asset& asset) override;
private:
  std::string mSourceVS;
  std::string mSourcePS;
};