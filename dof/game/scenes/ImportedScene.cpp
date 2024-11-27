#include "Precompile.h"
#include "scenes/ImportedScene.h"

#include "SceneNavigator.h"

#include "AppBuilder.h"
#include "scenes/LoadingScene.h"
#include "scenes/SceneList.h"
#include "SceneNavigator.h"
#include "loader/AssetHandle.h"
#include "loader/AssetLoader.h"
#include "loader/SceneAsset.h"

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

  struct SceneView {
    SceneView(RuntimeDatabaseTaskBuilder& task)
      : resolver{ task.getResolver(scene, stable) }
      , res{ task.getIDResolver()->getRefResolver() } {
    }

    const Loader::SceneAsset* tryGet(const Loader::AssetHandle& handle) {
      auto id = res.tryUnpack(handle.asset);
      return id ? resolver->tryGetOrSwapRowElement(scene, *id) : nullptr;
    }

    CachedRow<const Loader::SceneAssetRow> scene;
    CachedRow<const StableIDRow> stable;
    std::shared_ptr<ITableResolver> resolver;
    ElementRefResolver res;
  };

  void instantiateScene(IAppBuilder& builder) {
    auto task = builder.createTask();
    ImportedSceneGlobals* globals = getImportedSceneGlobals(task);
    SceneView sceneView{ task };

    task.setCallback([globals, sceneView](AppTaskArgs&) mutable {
      printf("instantiate scene\n");
      if(const Loader::SceneAsset* scene = sceneView.tryGet(globals->toLoad)) {
        printf("found scene");
        //TODO: load
      }


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
      , navigator{ Scenes::createLoadingNavigator(task) }
      , loader{ Loader::createAssetLoader(task) }
      , sceneID{ SceneList::get(task)->imported }
    {
    }

    Loader::AssetHandle importScene(Loader::AssetLocation&& location) final {
      const Loader::AssetHandle handle = loader->requestLoad(std::move(location));
      instantiateImportedScene(handle);
      return handle;
    }

    void instantiateImportedScene(const Loader::AssetHandle& scene) final {
      //Store the desired scene for ImportedScene to load
      globals->toLoad = scene;
      //Navigate to the loading screen and await the loading of the scene asset
      //Upon completion, navigate to the imported scene which will load from globals
      navigator->awaitLoadRequest(Scenes::LoadRequest{
        .toAwait = { scene },
        .onSuccess = sceneID,
        //TODO: error handling
        .onFailure = sceneID
      });
    }

    ImportedSceneGlobals* globals{};
    std::shared_ptr<Scenes::ILoadingNavigator> navigator;
    std::shared_ptr<Loader::IAssetLoader> loader;
    SceneNavigator::SceneID sceneID{};
    SceneNavigator::SceneID loadingScene{};
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
