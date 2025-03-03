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

    explicit operator bool() const {
      return use != nullptr;
    }

    bool operator==(const AssetHandle& h) const { return asset == h.asset; }
    bool operator!=(const AssetHandle& h) const { return !(*this == h); }

    ElementRef asset;
    UsageTrackerHandle use;
  };
};