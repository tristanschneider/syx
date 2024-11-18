#pragma once

namespace SceneNavigator {
  struct IScene;
}

class ElementRef;
class RuntimeDatabaseTaskBuilder;
struct IDatabase;
struct StableElementMappings;

namespace Scenes {
  struct IImportedSceneNavigator {
    virtual ~IImportedSceneNavigator() = default;
    //Navigate to a scene represented by the SceneAsset this ElementRef is pointing at
    virtual void instantiateImportedScene(const ElementRef& scene) = 0;
  };

  std::shared_ptr<IImportedSceneNavigator> createImportedSceneNavigator(RuntimeDatabaseTaskBuilder& task);

  std::unique_ptr<SceneNavigator::IScene> createImportedScene();
  std::unique_ptr<IDatabase> createImportedSceneDB(StableElementMappings& mappings);
}