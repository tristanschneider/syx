#pragma once

#include "StableElementID.h"

namespace Loader {
  struct UsageTracker {};
  using UsageTrackerHandle = std::shared_ptr<UsageTracker>;

  struct AssetHandle {
    ElementRef asset;
    UsageTrackerHandle use;
  };
};