#pragma once
#include "file/FileSystem.h"

class App;
class Asset;
struct AssetInfo;
struct SystemArgs;

enum class AssetLoadResult : uint8_t {
  NotFound,
  IOError,
  Fail,
  Success
};

class AssetLoader {
public:
  AssetLoader(const std::string& category);
  virtual ~AssetLoader();

  const std::string& getCategory();
  virtual AssetLoadResult load(const std::string& basePath, Asset& asset) = 0;
  //Not required. Gives a chance to kick off any post processing tasks after successful loading. User is responsible for setting PostProcessed state
  virtual void postProcess(const SystemArgs&, Asset&) {}

protected:

  static AssetLoadResult _readEntireFile(const std::string& filename, std::vector<uint8_t> buffer) {
    using namespace FileSystem;
    FileResult result = get().readFile(filename.c_str(), buffer);
    switch(result) {
      default:
      case FileResult::Fail: return AssetLoadResult::Fail;
      case FileResult::IOError: return AssetLoadResult::IOError;
      case FileResult::NotFound: return AssetLoadResult::NotFound;
      case FileResult::Success: return AssetLoadResult::Success;
    }
  }

  template<typename Buffer>
  static AssetLoadResult _readEntireFile(const std::string& filename, Buffer& buffer) {
    std::vector<uint8_t> temp;
    const AssetLoadResult result = _readEntireFile(filename, temp);
    buffer.resize(temp.size());
    std::memcpy(buffer.data(), temp.data(), temp.size());
    return result;
  }


private:
  std::string mCategory;
};

class BufferAssetLoader : public AssetLoader {
public:
  using AssetLoader::AssetLoader;
  virtual ~BufferAssetLoader();

  //Load file in to mData
  virtual AssetLoadResult load(const std::string& basePath, Asset& asset) override;

protected:
  //Do whatever processing and put result in asset
  virtual AssetLoadResult _load(Asset& asset);

  std::vector<uint8_t> mData;
};

class TextAssetLoader : public AssetLoader {
public:
  TextAssetLoader(const std::string& category);
  virtual ~TextAssetLoader();

  virtual AssetLoadResult load(const std::string& basePath, Asset& asset) override;

protected:
  virtual AssetLoadResult _load(Asset& asset);
  //Mirrors std::ifstream::getline
  size_t _getLine(char* buffer, size_t bufferSize, char delimiter = '\n');

  std::string mData;
  size_t mCurIndex;
};