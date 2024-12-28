#pragma once

#include "generics/RateLimiter.h"
#include "loader/AssetLoader.h"
#include "loader/AssetReader.h"
#include "Table.h"

namespace Loader {
  struct AssetLoadTask;

  //Assets are grouped into separate tables based on their load state
  struct RequestedTagRow : TagRow{};
  struct SucceededTagRow : TagRow{};
  struct LoadingTagRow : TagRow{};
  struct FailedTagRow : TagRow{};

  struct UsageTrackerBlock {
    std::weak_ptr<UsageTracker> tracker;
  };
  struct UsageTrackerBlockRow : Row<UsageTrackerBlock> {};

  struct LoadRequest {
    AssetLocation location;
    //Optional, may be empty
    std::vector<uint8_t> contents;
  };
  struct LoadRequestRow : Row<LoadRequest> {};

  struct Globals {
    gnx::OneInTenRateLimit assetCompletionLimit, assetGCLimit;
  };
  struct GlobalsRow : SharedRow<Globals> {};

  struct LoadingAsset {
    ~LoadingAsset();

    std::shared_ptr<AssetLoadTask> task;
    LoadState state;
  };
  struct LoadingAssetRow : Row<LoadingAsset> {};

  struct KnownTables {
    KnownTables(RuntimeDatabaseTaskBuilder& task);

    TableID requests;
    TableID loading;
    TableID failed;
    std::vector<TableID> succeeded;
  };
};