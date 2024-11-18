#include "Precompile.h"
#include "scenes/ImportedScene.h"

#include "SceneNavigator.h"

#include "AppBuilder.h"
#include "scenes/SceneList.h"
#include "SceneNavigator.h"

namespace Scenes {
  struct ImportedSceneGlobals {
    ElementRef toLoad;
  };
  struct ImportedSceneGlobalsRow : SharedRow<ImportedSceneGlobals> {};
  using ImportedSceneDB = Database<
    Table<ImportedSceneGlobalsRow>
  >;

  struct ImportedScene : SceneNavigator::IScene {
    void init(IAppBuilder& builder) final {
      //TODO: insert objects from SceneAsset here
      builder;
    }

    //Scene manages itself based on what was loaded
    void update(IAppBuilder&) final {}

    //Contents of scene are unloaded by the IsClearedWithSceneTag
    void uninit(IAppBuilder&) final {}
  };

  struct ImportedSceneNavigator : IImportedSceneNavigator {
    ImportedSceneNavigator(RuntimeDatabaseTaskBuilder& task)
      : globals{ task.query<ImportedSceneGlobalsRow>().tryGetSingletonElement<0>() }
      , navigator{ SceneNavigator::createNavigator(task) }
      , sceneID{ SceneList::get(task)->imported }
    {
    }

    void instantiateImportedScene(const ElementRef& scene) final {
      // TODO: go to loading scene first to wait for it to finish?
      globals->toLoad = scene;
      navigator->navigateTo(sceneID);
    }

    ImportedSceneGlobals* globals{};
    std::shared_ptr<SceneNavigator::INavigator> navigator;
    SceneNavigator::SceneID sceneID{};
  };

  std::shared_ptr<IImportedSceneNavigator> createImportedSceneNavigator(RuntimeDatabaseTaskBuilder& task) {
    return std::make_shared<ImportedSceneNavigator>(task);
  }

  std::unique_ptr<SceneNavigator::IScene> createImportedScene() {
    return std::make_unique<ImportedScene>();
  }

  std::unique_ptr<IDatabase> createImportedSceneDB(StableElementMappings& mappings) {
    return DBReflect::createDatabase<ImportedSceneDB>(mappings);
  }
}
