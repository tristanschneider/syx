#include "Precompile.h"
#include "asset/AssetLoader.h"

AssetLoader::AssetLoader(const std::string& category)
  : mCategory(category) {
}

AssetLoader::~AssetLoader() {
}

const std::string& AssetLoader::getCategory() {
  return mCategory;
}

AssetLoadResult AssetLoader::load(Asset& asset) {
  return AssetLoadResult::Fail;
}
