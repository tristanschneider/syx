#pragma once

#include "loader/AssetHandle.h"

class RuntimeDatabaseTaskBuilder;

namespace Loader {
  struct AssetLocation {
    auto operator<=>(const AssetLocation&) const = default;

    std::string filename;
  };

  class IAssetLoader {
  public:
    virtual ~IAssetLoader() = default;
    //Starts the loading process of the desired asset.
    //Its progress can be checked by using the IAssetReader to check the load state or
    //Checking that the element is in the expected table
    virtual AssetHandle requestLoad(AssetLocation&& location) = 0;
    //Workaround for easier testing and built-in assets
    virtual AssetHandle requestLoad(AssetLocation&& location, std::vector<uint8_t>&& contents) = 0;
  };

  std::shared_ptr<IAssetLoader> createAssetLoader(RuntimeDatabaseTaskBuilder& task);
}

namespace std {
  template<>
  struct hash<Loader::AssetLocation> {
    size_t operator()(const Loader::AssetLocation& v) const {
      return std::hash<std::string>{}(v.filename);
    }
  };
}