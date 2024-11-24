#include "Precompile.h"
#include "scenes/ImportedScene.h"

#include "SceneNavigator.h"

#include "AppBuilder.h"
#include "scenes/SceneList.h"
#include "SceneNavigator.h"
#include "loader/AssetHandle.h"

namespace Scenes {
  struct ImportedSceneGlobals {
    Loader::AssetHandle toLoad;
  };
  struct ImportedSceneGlobalsRow : SharedRow<ImportedSceneGlobals> {};
  using ImportedSceneDB = Database<
    Table<ImportedSceneGlobalsRow>
  >;

  ImportedSceneGlobals* getImportedSceneGlobals(RuntimeDatabaseTaskBuilder& task) {
    auto result = task.query<ImportedSceneGlobalsRow>().tryGetSingletonElement();
    assert(result);
    return result;
  }

  void instantiateScene(IAppBuilder& builder) {
    auto task = builder.createTask();
    ImportedSceneGlobals* globals = getImportedSceneGlobals(task);

    task.setCallback([globals](AppTaskArgs&) {
      //TODO: load

      //Load is finished, release asset handle
      globals->toLoad = {};
    });

    builder.submitTask(std::move(task.setName("instantiate scene")));
  }

  struct ImportedScene : SceneNavigator::IScene {
    void init(IAppBuilder& builder) final {
      instantiateScene(builder);
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

    void importScene(Loader::AssetLocation&& location) final {
     //TODO: request load then instantiateImportedScene
      location;
    }

    void instantiateImportedScene(const Loader::AssetHandle& scene) final {
      //TODO: go to loading scene first to wait for it to finish?
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
