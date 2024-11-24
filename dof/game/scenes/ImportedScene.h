#pragma once

namespace SceneNavigator {
  struct IScene;
}

namespace Loader {
  struct AssetLocation;
  struct AssetHandle;
};

class ElementRef;
class RuntimeDatabaseTaskBuilder;
struct IDatabase;
struct StableElementMappings;

namespace Scenes {
  struct IImportedSceneNavigator {
    virtual ~IImportedSceneNavigator() = default;
    //Load a scene by name then instantiate it
    virtual void importScene(Loader::AssetLocation&& location) = 0;
    //Navigate to a scene represented by the SceneAsset this Handle is pointing at
    virtual void instantiateImportedScene(const Loader::AssetHandle& scene) = 0;
  };

  std::shared_ptr<IImportedSceneNavigator> createImportedSceneNavigator(RuntimeDatabaseTaskBuilder& task);

  std::unique_ptr<SceneNavigator::IScene> createImportedScene();
  std::unique_ptr<IDatabase> createImportedSceneDB(StableElementMappings& mappings);
}