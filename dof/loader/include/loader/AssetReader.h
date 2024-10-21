#pragma once

class StableElementID;
class RuntimeDatabaseTaskBuilder;

namespace Loader {
  struct AssetHandle;

  enum class LoadStep : uint8_t {
    Requested,
    Loading,
    Succeeded,
    //Valid asset that failed to load
    Failed,
    //AssetHandle isn't pointing at any asset
    Invalid
  };

  struct LoadState {
    LoadStep step{};
  };

  class IAssetReader {
  public:
    virtual ~IAssetReader() = default;
    virtual LoadState getLoadState(const AssetHandle& asset) = 0;
  };

  std::shared_ptr<IAssetReader> createAssetReader(RuntimeDatabaseTaskBuilder& task);
}