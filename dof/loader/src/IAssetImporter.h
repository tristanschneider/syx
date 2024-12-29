#pragma once

namespace Loader {
  struct LoadRequest;
  struct AssetVariant;

  class IAssetImporter {
  public:
    virtual ~IAssetImporter() = default;
    virtual bool isSupportedExtension(std::string_view extension) = 0;
    virtual void loadAsset(const Loader::LoadRequest& request, AssetVariant& result) = 0;
  };
}