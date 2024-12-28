#pragma once

#include "StableElementID.h"

namespace Loader {
  struct UsageTracker {};
  using UsageTrackerHandle = std::shared_ptr<UsageTracker>;

  struct AssetHandle {
    //Asset handles are normally pointing at UsageTrackerBlockRow. This creates a temporary one to be put into the destination
    //UsageTrackerBlockRow later.
    static AssetHandle createPending(const ElementRef& e) {
      return AssetHandle{
        .asset = e,
        .use = std::make_shared<UsageTracker>(),
      };
    }

    ElementRef asset;
    UsageTrackerHandle use;
  };
};