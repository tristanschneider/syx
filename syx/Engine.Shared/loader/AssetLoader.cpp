#include "Precompile.h"
#include "loader/AssetLoader.h"
#include "asset/Asset.h"

AssetLoader::AssetLoader(FileSystem::IFileSystem& fileSystem, const std::string& category)
  : mCategory(category)
  , mFileSystem(fileSystem) {
}

AssetLoader::~AssetLoader() {
}

const std::string& AssetLoader::getCategory() {
  return mCategory;
}

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

TextAssetLoader::~TextAssetLoader() = default;

AssetLoadResult TextAssetLoader::load(const std::string& basePath, Asset& asset) {
  mCurIndex = 0;
  std::string fullPath = basePath + asset.getInfo().mUri;
  AssetLoadResult ioResult = _readEntireFile(fullPath, mData);
  return ioResult == AssetLoadResult::Success ? _load(asset) : ioResult;
}

AssetLoadResult TextAssetLoader::_load(Asset& asset) {
  static_cast<TextAsset&>(asset).set(std::move(mData));
  return AssetLoadResult::Success;
}

size_t TextAssetLoader::_getLine(char* buffer, size_t bufferSize, char delimiter) {
  size_t readCount = 0;
  while(readCount < bufferSize && mCurIndex < mData.size()) {
    char c = mData[mCurIndex++];
    buffer[readCount++] = c;
    if(c == delimiter)
      break;
  }
  return readCount;
}
