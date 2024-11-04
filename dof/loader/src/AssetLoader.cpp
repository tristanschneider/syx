#include "Precompile.h"
#include "loader/AssetLoader.h"

#include "AssetTables.h"
#include "AppBuilder.h"

namespace Loader {
  class AssetLoader : public IAssetLoader {
  public:
    AssetLoader(RuntimeDatabaseTaskBuilder& task)
    {
      auto q = task.query<const RequestedTagRow, const StableIDRow, LoadRequestRow, UsageTrackerBlockRow>();
      assert(q.size());
      modifier = task.getModifierForTable(q.matchingTableIDs[0]);
      requests = &q.get<2>(0);
      ids = &q.get<1>(0);
      usageTracker = &q.get<3>(0);
    }

    AssetHandle requestLoad(AssetLocation&& location) final {
      const size_t i = modifier->addElements(1);
      LoadRequest& request = requests->at(i);
      AssetHandle result{ .asset{ ids->at(i) }, .use{ std::make_shared<UsageTracker>() } };
      usageTracker->at(i).tracker = result.use;
      request.location = std::move(location);
      return result;
    }

    AssetHandle requestLoad(AssetLocation&& location, std::vector<uint8_t>&& contents) final {
      const size_t i = modifier->addElements(1);
      LoadRequest& request = requests->at(i);
      AssetHandle result{ .asset{ ids->at(i) }, .use{ std::make_shared<UsageTracker>() } };
      usageTracker->at(i).tracker = result.use;
      request.location = std::move(location);
      request.contents = std::move(contents);
      return result;
    }

    std::shared_ptr<ITableModifier> modifier;
    LoadRequestRow* requests{};
    UsageTrackerBlockRow* usageTracker{};
    const StableIDRow* ids{};
  };

  std::shared_ptr<IAssetLoader> createAssetLoader(RuntimeDatabaseTaskBuilder& task) {
    return std::make_shared<AssetLoader>(task);
  }
}