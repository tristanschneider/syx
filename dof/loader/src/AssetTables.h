#pragma once

#include "generics/RateLimiter.h"
#include "loader/AssetLoader.h"
#include "loader/AssetReader.h"
#include "Table.h"

namespace Loader {
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
};