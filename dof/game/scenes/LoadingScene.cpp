#include "Precompile.h"
#include "scenes/LoadingScene.h"

#include "AppBuilder.h"
#include "SceneNavigator.h"
#include "Simulation.h"
#include "SceneList.h"
#include "loader/AssetReader.h"
#include "loader/AssetLoader.h"
#include "GraphicsTables.h"

namespace Scenes {
  struct LoadingSceneGlobals {
    LoadRequest currentRequest;
  };
  struct LoadingSceneGlobalsRow : SharedRow<LoadingSceneGlobals> {};
  using LoadingSceneDB = Database<Table<LoadingSceneGlobalsRow>>;

  constexpr bool ENABLE_LOAD_LOG = true;
  template<class... Args>
  void loadLog([[maybe_unused]] const char* format, [[maybe_unused]] const Args&... args) {
    if constexpr(ENABLE_LOAD_LOG) {
      printf(format, args...);
    }
  }

  LoadingSceneGlobals* getLoadingSceneGlobals(RuntimeDatabaseTaskBuilder& task) {
    LoadingSceneGlobals* globals = task.query<LoadingSceneGlobalsRow>().tryGetSingletonElement();
    assert(globals);
    return globals;
  }

  void initRequestAssets(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("request assets");
    SceneState* sceneState = task.query<SharedRow<SceneState>>().tryGetSingletonElement();
    FileSystem* fs = task.query<SharedRow<FileSystem>>().tryGetSingletonElement();
    std::shared_ptr<Loader::IAssetLoader> loader = Loader::createAssetLoader(task);
    if(!sceneState || !fs) {
      task.discard();
      return;
    }
    auto playerTextures = task.query<SharedTextureRow, const IsPlayer>();
    auto fragmentTextures = task.query<SharedTextureRow, const IsFragment>();
    auto terrain = task.query<SharedTextureRow, const Tags::TerrainRow>();
    LoadingSceneGlobals* globals = getLoadingSceneGlobals(task);

    task.setCallback([=](AppTaskArgs&) mutable {
      loadLog("Loading...");
      //TODO: less hacky way to queue the initial request, probably make the caller figure out the necessary assets
      if(!globals->currentRequest.doInitialLoad) {
        return;
      }
      const std::string& root = fs->mRoot;
      const Loader::AssetHandle backgroundImage = loader->requestLoad({ root + "background.png" });
      const Loader::AssetHandle playerImage = loader->requestLoad({ root + "player.png" });
      const Loader::AssetHandle groundImage = loader->requestLoad({ root + "ground.png" });

      for(size_t i = 0; i < playerTextures.size(); ++i) {
        playerTextures.get<0>(i).at().asset = playerImage;
      }
      //Make all the objects use the background image as their texture
      for(size_t i = 0; i < fragmentTextures.size(); ++i) {
        fragmentTextures.get<0>(i).at().asset = backgroundImage;
      }
      for(size_t i = 0; i < terrain.size(); ++i) {
        terrain.get<0>(i).at().asset = groundImage;
      }
    });
    builder.submitTask(std::move(task));
  }

  void awaitAssetLoading(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("Await Assets");
    SceneState* sceneState = task.query<SharedRow<SceneState>>().tryGetSingletonElement();
    LoadingSceneGlobals* globals = getLoadingSceneGlobals(task);
    auto assetReader = Loader::createAssetReader(task);
    auto nav = SceneList::createNavigator(task);
    if(!sceneState) {
      task.discard();
      return;
    }

    task.setCallback([sceneState, nav, assetReader, globals](AppTaskArgs&) mutable {
      loadLog(".");

      SceneNavigator::SceneID toScene = globals->currentRequest.onSuccess;
      for(const Loader::AssetHandle& asset : globals->currentRequest.toAwait) {
        switch(assetReader->getLoadState(asset).step) {
          case Loader::LoadStep::Requested:
          case Loader::LoadStep::Loading:
            //Still in progress, try again later
            return;

          case Loader::LoadStep::Succeeded:
            continue;
          case Loader::LoadStep::Failed:
          case Loader::LoadStep::Invalid:
            //Something failed, stop and go to failure scene
            toScene = globals->currentRequest.onFailure;
            break;
        }
      }

      //If they're all done, continue on to the next phase
      nav.navigator->navigateTo(toScene);
    });
    builder.submitTask(std::move(task));
  }

  void clearLoadRequest(IAppBuilder& builder) {
    auto task = builder.createTask();
    LoadingSceneGlobals* globals = getLoadingSceneGlobals(task);

    task.setCallback([globals](AppTaskArgs&) {
      loadLog("\nDone.\n");
      globals->currentRequest = {};
    });

    builder.submitTask(std::move(task.setName("clear load request")));
  }

  struct LoadingScene : SceneNavigator::IScene {
    void init(IAppBuilder& builder) final {
      initRequestAssets(builder);
    }
    void update(IAppBuilder& builder) final {
      awaitAssetLoading(builder);
    }
    void uninit(IAppBuilder& builder) final {
      clearLoadRequest(builder);
    }
  };

  std::unique_ptr<SceneNavigator::IScene> createLoadingScene() {
    return std::make_unique<LoadingScene>();
  }

  struct LoadingNavigator : ILoadingNavigator {
    LoadingNavigator(RuntimeDatabaseTaskBuilder& task)
      : globals{ getLoadingSceneGlobals(task) }
      , navigator{ SceneNavigator::createNavigator(task) }
      , scenes{ SceneList::get(task) }
    {
    }

    void awaitLoadRequest(LoadRequest&& request) final {
      //Store the request so the loading screen knows what to load
      globals->currentRequest = std::move(request);
      //Go to the loading screen
      navigator->navigateTo(scenes->loading);
    }

    LoadingSceneGlobals* globals{};
    std::shared_ptr<SceneNavigator::INavigator> navigator;
    const SceneList::Scenes* scenes{};
  };

  std::shared_ptr<ILoadingNavigator> createLoadingNavigator(RuntimeDatabaseTaskBuilder& task) {
    return std::make_shared<LoadingNavigator>(task);
  }

  void createLoadingSceneDB(RuntimeDatabaseArgs& args) {
    DBReflect::addDatabase<LoadingSceneDB>(args);
  }
}