#pragma once

class Asset;
struct AssetInfo;

enum class AssetLoadResult : uint8_t {
  NotFound,
  Fail,
  Success
};

class AssetLoader {
public:
  AssetLoader(const std::string& category);
  virtual ~AssetLoader();

  const std::string& getCategory();
  //Opens file according to configuration then calls the appropriate _load
  AssetLoadResult load(Asset& asset);

private:
  std::string mCategory;
};