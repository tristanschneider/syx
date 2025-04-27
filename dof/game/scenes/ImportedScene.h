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
struct RuntimeDatabaseArgs;
class IAppModule;

namespace Scenes {
  struct IImportedSceneNavigator {
    virtual ~IImportedSceneNavigator() = default;
    //Load a scene by name then instantiate it
    //Caller may retain the handle if they want this scene definition to persist,
    //the loader will retain it long enough to enter the scene if the return value is ignored
    virtual Loader::AssetHandle importScene(Loader::AssetLocation&& location) = 0;
    //Navigate to a scene represented by the SceneAsset this Handle is pointing at
    virtual void instantiateImportedScene(const Loader::AssetHandle& scene) = 0;
  };

  std::shared_ptr<IImportedSceneNavigator> createImportedSceneNavigator(RuntimeDatabaseTaskBuilder& task);

  std::unique_ptr<SceneNavigator::IScene> createImportedScene();
  void createImportedSceneDB(RuntimeDatabaseArgs& args);
}

namespace BasicLoaders {
  std::unique_ptr<IAppModule> createModule();
}