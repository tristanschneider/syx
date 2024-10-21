#include "Precompile.h"
#include "loader/AssetReader.h"

#include "AssetTables.h"
#include "AppBuilder.h"

namespace Loader {
  class AssetReader : public IAssetReader {
  public:
    AssetReader(RuntimeDatabaseTaskBuilder& task)
      : resolver{ task.getResolver(
          requested,
          succeeded,
          loading,
          failed
      )}
      , res{ task.getIDResolver()->getRefResolver() }
    {
    }

    LoadState getLoadState(const AssetHandle& asset) final {
      const auto id = res.tryUnpack(asset.asset);
      if(!id) {
        return { LoadStep::Invalid };
      }
      if(resolver->tryGetOrSwapRow(succeeded, *id)) {
        return { LoadStep::Succeeded };
      }
      if(resolver->tryGetOrSwapRow(loading, *id)) {
        return { LoadStep::Loading };
      }
      if(resolver->tryGetOrSwapRow(requested, *id)) {
        return { LoadStep::Requested };
      }
      if(resolver->tryGetOrSwapRow(failed, *id)) {
        return { LoadStep::Failed };
      }
      return { LoadStep::Invalid };
    }

    ElementRefResolver res;
    std::shared_ptr<ITableResolver> resolver;
    CachedRow<const RequestedTagRow> requested;
    CachedRow<const SucceededTagRow> succeeded;
    CachedRow<const LoadingTagRow> loading;
    CachedRow<const FailedTagRow> failed;
  };

  std::shared_ptr<IAssetReader> createAssetReader(RuntimeDatabaseTaskBuilder& task) {
    return std::make_shared<AssetReader>(task);
  }
}