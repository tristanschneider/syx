#include "Precompile.h"
#include "asset/ImmediateAssetWrapper.h"

#include "asset/Asset.h"
#include "event/AssetEvents.h"
#include "event/EventBuffer.h"
#include "system/AssetRepo.h"

namespace {
  struct ImmediateAssetWrapper : public Asset {
    using Asset::Asset;

    static void init(std::shared_ptr<ImmediateAssetWrapper> self, EventBuffer& msg, typeId_t<System> systemResponseHandler) {
      msg.push(GetAssetRequest(self->getInfo()).then(systemResponseHandler, [self](const GetAssetResponse& e) {
        self->mWrappedAsset = e.mAsset;
      }));
    }

    const Asset* _tryUnwrap() const override {
      return mWrappedAsset ? mWrappedAsset.get() : nullptr;
    }

    AssetState getState() const override {
      return mWrappedAsset ? mWrappedAsset->getState() : AssetState::Empty;
    }

    const AssetInfo& getInfo() const override {
      return mWrappedAsset ? mWrappedAsset->getInfo() : Asset::getInfo();
    };

    std::shared_ptr<Asset> mWrappedAsset;
  };
}

namespace ImmediateAsset {
  std::shared_ptr<Asset> create(AssetInfo info, EventBuffer& msg, typeId_t<System> systemResponseHandler) {
    std::shared_ptr<ImmediateAssetWrapper> result = AssetRepo::createAsset<ImmediateAssetWrapper>(std::move(info));
    result->init(result, msg, systemResponseHandler);
    return result;
  }
};