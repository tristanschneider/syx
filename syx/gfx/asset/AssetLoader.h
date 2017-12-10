#pragma once

class Asset;
struct AssetInfo;

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
  using AssetLoader::AssetLoader;
  virtual ~TextAssetLoader();

  virtual AssetLoadResult load(const std::string& basePath, Asset& asset) override;

protected:
  virtual AssetLoadResult _load(Asset& asset);

  std::string mData;
};