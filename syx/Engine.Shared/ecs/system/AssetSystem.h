#pragma once

#include "ecs/component/AssetComponent.h"
#include "ecs/component/FileSystemComponent.h"
#include "ecs/ECS.h"
#include <variant>

//Unsupported by this loader, continue trying others
struct UnsupportedAssetTag {};
//Supported by this loader but failed to load, mark as failed asset
struct FailedAssetTag {};

template<class AssetT>
using AssetLoadResultV = std::variant<AssetT, UnsupportedAssetTag, FailedAssetTag>;

//TODO: this is pretty similar to scene loading, scene loading should use this
struct AssetSystem {
  //Take AssetLoadRequest, adds asset component and requests IO read
  static std::shared_ptr<Engine::System> processLoadRequests();
  //Destroys the entity of assets that failed to load
  static std::shared_ptr<Engine::System> deleteFailedAssets();

  static std::shared_ptr<Engine::System> createGraphicsModelLoader();
  static std::shared_ptr<Engine::System> createShaderLoader();
  static std::shared_ptr<Engine::System> createTextureLoader();

  //TODO: how to clean up request if no loaders support the asset type?
  //TODO: restrict loads to within the project and make the path relative with the project locator

  template<class R>
  static R _getLoadResult(AssetLoadResultV<R>(*)(const AssetInfoComponent&, std::vector<uint8_t>&));

  //Template for asset loaders that can instantly load an asset from a buffer
  //More involved loaders should view the asset with the FileReadSuccessResponse,
  //perform the desired loading steps, and add the AssetComponent when successful
  //Loader is std::optional<AssetT>(*loader)(std::vector<uint8_t>&)
  template<auto loader>
  static std::shared_ptr<Engine::System> instantAssetLoadSystem(std::string name) {
    return intermediateAssetLoadSystem<loader, AssetComponent>(std::move(name));
  }

  template<auto loader, class... ToAdd>
  static std::shared_ptr<Engine::System> intermediateAssetLoadSystem(std::string name) {
    using AssetT = decltype(_getLoadResult(loader));
    using View = Engine::View<Engine::Include<AssetLoadRequestComponent>, Engine::Read<AssetInfoComponent>, Engine::Write<FileReadSuccessResponse>, Engine::Exclude<AssetT>, Engine::Exclude<AssetComponent>, Engine::Exclude<AssetLoadFailedComponent>>;
    using Modifier = Engine::EntityModifier<AssetComponent, AssetLoadFailedComponent, AssetT, ToAdd...>;
    return ecx::makeSystem(std::move(name), [](Engine::SystemContext<View, Modifier>& context) {
      View& view = context.get<View>();
      Modifier modifier = context.get<Modifier>();
      for(auto viewedEntity : view) {
        Engine::Entity entity = viewedEntity.entity();
        FileReadSuccessResponse& file = viewedEntity.get<FileReadSuccessResponse>();
        const AssetInfoComponent& info = viewedEntity.get<const AssetInfoComponent>();

        AssetLoadResultV<AssetT> loaded = loader(info, file.mBuffer);
        if(AssetT* asset = std::get_if<AssetT>(&loaded)) {
          modifier.addComponent<AssetT>(entity, std::move(*asset));
          (modifier.addComponent<ToAdd>(entity), ...);
        }
        else if(std::holds_alternative<FailedAssetTag>(loaded)) {
          modifier.addComponent<AssetLoadFailedComponent>(entity);
        }
        //else if is an unsupported format, don't change the entity
      }
    });
  }
};