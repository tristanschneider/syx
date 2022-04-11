#pragma once

#include "ecs/component/AssetComponent.h"
#include "ecs/component/FileSystemComponent.h"
#include "ecs/ECS.h"

//TODO: this is pretty similar to scene loading, scene loading should use this
struct AssetSystem {
  //Take AssetLoadRequest, adds asset component and requests IO read
  static std::shared_ptr<Engine::System> processLoadRequests();
  //Destroys the entity of assets that failed to load
  static std::shared_ptr<Engine::System> deleteFailedAssets();

  template<class R>
  static R _getLoadResult(std::optional<R>(*)(std::vector<uint8_t>&));

  //Template for asset loaders that can instantly load an asset from a buffer
  //More involved loaders should view the asset with the FileReadSuccessResponse,
  //perform the desired loading steps, and add the AssetComponent when successful
  //Loader is std::optional<AssetT>(*loader)(std::vector<uint8_t>&)
  template<auto loader>//class AssetT, std::optional<AssetT>(*loader)(std::vector<uint8_t>&)>
  static std::shared_ptr<Engine::System> instantAssetLoadSystem(std::string name) {
    using AssetT = decltype(_getLoadResult(loader));
    using View = Engine::View<Engine::Include<AssetLoadRequestComponent>, Engine::Write<FileReadSuccessResponse>>;
    using Modifier = Engine::EntityModifier<AssetComponent, AssetLoadFailedComponent, AssetT>;
    return ecx::makeSystem(std::move(name), [](Engine::SystemContext<View, Modifier>& context) {
      View& view = context.get<View>();
      Modifier modifier = context.get<Modifier>();
      for(auto viewedEntity : view) {
        Engine::Entity entity = viewedEntity.entity();
        FileReadSuccessResponse& file = viewedEntity.get<FileReadSuccessResponse>();

        if(std::optional<AssetT> loaded = loader(file.mBuffer)) {
          modifier.addComponent<AssetT>(entity, std::move(*loaded));
          modifier.addComponent<AssetComponent>(entity);
        }
        else {
          modifier.addComponent<AssetLoadFailedComponent>(entity);
        }
      }
    });
  }
};