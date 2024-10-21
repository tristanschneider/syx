#pragma once

#include "loader/AssetHandle.h"

class RuntimeDatabaseTaskBuilder;

namespace Loader {
  struct AssetLocation {
    std::string filename;
  };

  class IAssetLoader {
  public:
    virtual ~IAssetLoader() = default;
    //Starts the loading process of the desired asset.
    //Its progress can be checked by using the IAssetReader to check the load state or
    //Checking that the element is in the expected table
    virtual AssetHandle requestLoad(AssetLocation&& location) = 0;
  };

  std::shared_ptr<IAssetLoader> createAssetLoader(RuntimeDatabaseTaskBuilder& task);
}