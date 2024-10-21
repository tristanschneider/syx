#pragma once

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

  //Loading assets are stored with a different type than finished ones
  //so that a query for the expected asset type only results in finshed assets
  //template<class T>
  //struct LoadingAsset : T {
  //  LoadState state;
  //};
  //template<IsRow T>
  //struct LoadingAssetRow : T {};

  struct LoadRequest {
    AssetLocation location;
  };
  struct LoadRequestRow : Row<LoadRequest> {};
};