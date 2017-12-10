#include "Precompile.h"
#include "asset/AssetLoader.h"
#include "asset/AssetRepo.h"
#include "asset/Asset.h"

namespace {
  template<typename Buffer>
  AssetLoadResult _readEntireFile(const std::string& filename, Buffer& buffer) {
    std::FILE* file = std::fopen(filename.c_str(), "rb");
    if (!file)
      return AssetLoadResult::NotFound;

    std::fseek(file, 0, SEEK_END);
    long bytes = std::ftell(file);
    std::rewind(file);
    buffer.clear();
    buffer.resize(static_cast<size_t>(bytes));

    bool readSuccess = bytes == std::fread(&buffer[0], 1, bytes, file);
    std::fclose(file);
    return readSuccess ? AssetLoadResult::Success : AssetLoadResult::IOError;
  }
}

AssetLoader::AssetLoader(const std::string& category)
  : mCategory(category) {
}

AssetLoader::~AssetLoader() {
}

const std::string& AssetLoader::getCategory() {
  return mCategory;
}

RegisterAssetLoader("buff", BufferAssetLoader, BufferAsset);

BufferAssetLoader::~BufferAssetLoader() {
}

AssetLoadResult BufferAssetLoader::load(const std::string& basePath, Asset& asset) {
  std::string fullPath = basePath + asset.getInfo().mUri;
  AssetLoadResult ioResult = _readEntireFile(fullPath, mData);
  return ioResult == AssetLoadResult::Success ? _load(asset) : ioResult;
}

AssetLoadResult BufferAssetLoader::_load(Asset& asset) {
  static_cast<BufferAsset&>(asset).set(std::move(mData));
  return AssetLoadResult::Success;
}

RegisterAssetLoader("txt", TextAssetLoader, TextAsset);

TextAssetLoader::~TextAssetLoader() {
}

AssetLoadResult TextAssetLoader::load(const std::string& basePath, Asset& asset) {
  std::string fullPath = basePath + asset.getInfo().mUri;
  AssetLoadResult ioResult = _readEntireFile(fullPath, mData);
  return ioResult == AssetLoadResult::Success ? _load(asset) : ioResult;
}

AssetLoadResult TextAssetLoader::_load(Asset& asset) {
  static_cast<TextAsset&>(asset).set(std::move(mData));
  return AssetLoadResult::Success;
}
