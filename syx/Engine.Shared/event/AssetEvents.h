#pragma once

#include "asset/Asset.h"
#include "event/Event.h"

struct GetAssetResponse : public TypedEvent<GetAssetResponse> {
  GetAssetResponse(std::shared_ptr<Asset> asset)
    : mAsset(std::move(asset)) {
  }

  std::shared_ptr<Asset> mAsset;
};

struct GetAssetRequest : public RequestEvent<GetAssetRequest, GetAssetResponse> {
  GetAssetRequest(AssetInfo info)
    : mInfo(std::move(info)) {
  }

  AssetInfo mInfo;
};

struct ReloadAssetResponse : public TypedEvent<ReloadAssetResponse> {
  ReloadAssetResponse(std::shared_ptr<Asset> asset, bool wasNewlyCreated)
    : mAsset(std::move(asset))
    , mWasNewlyCreated(wasNewlyCreated) {
  }

  std::shared_ptr<Asset> mAsset;
  bool mWasNewlyCreated = false;
};

struct ReloadAssetRequest : public RequestEvent<ReloadAssetRequest, ReloadAssetResponse> {
  ReloadAssetRequest(AssetInfo info)
    : mInfo(std::move(info)) {
  }

  AssetInfo mInfo;
};

struct AssetQueryResponse {
  AssetQueryResponse() = default;
  AssetQueryResponse(std::vector<std::shared_ptr<Asset>> results)
    : mResults(std::move(results)) {
  }

  std::vector<std::shared_ptr<Asset>> mResults;
};

struct AssetQueryRequest : public RequestEvent<AssetQueryRequest, AssetQueryResponse> {
  AssetQueryRequest(std::string category)
    : mCategory(std::move(category)) {
  }

  std::string mCategory;
};

struct AddAssetRequest : public TypedEvent<AddAssetRequest> {
  AddAssetRequest(std::shared_ptr<Asset> asset)
    : mAsset(std::move(asset)) {
  }

  std::shared_ptr<Asset> mAsset;
};