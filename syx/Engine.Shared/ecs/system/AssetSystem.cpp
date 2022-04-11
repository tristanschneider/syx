#include "Precompile.h"
#include "ecs/system/AssetSystem.h"

namespace Assets {
  using namespace Engine;
  using IOFailView = View<Include<AssetLoadRequestComponent>, Include<FileReadFailureResponse>>;
  using AssetFailView = View<Include<AssetLoadFailedComponent>>;
  void tickDelete(SystemContext<IOFailView, AssetFailView, EntityFactory>& context) {
    auto& io = context.get<IOFailView>();
    auto& failedAssets = context.get<AssetFailView>();
    EntityFactory factory = context.get<EntityFactory>();
    while(auto first = io.tryGetFirst()) {
      factory.destroyEntity(first->entity());
    }
    while(auto first = failedAssets.tryGetFirst()) {
      factory.destroyEntity(first->entity());
    }
  }

  using RequestView = View<Read<AssetLoadRequestComponent>, Exclude<FileReadRequest>>;
  using RequestModifier = EntityModifier<FileReadRequest>;
  void tickRequests(SystemContext<RequestView, RequestModifier>& context) {
    RequestModifier modifier = context.get<RequestModifier>();
    for(auto request : context.get<RequestView>()) {
       modifier.addComponent<FileReadRequest>(request.entity(), request.get<const AssetLoadRequestComponent>().mPath);
    }
  }
}

std::shared_ptr<Engine::System> AssetSystem::processLoadRequests() {
  return ecx::makeSystem("ProcessAssetLoadRequest", &Assets::tickRequests);
}

std::shared_ptr<Engine::System> AssetSystem::deleteFailedAssets() {
  return ecx::makeSystem("DeleteFailedAssets", &Assets::tickDelete);
}
