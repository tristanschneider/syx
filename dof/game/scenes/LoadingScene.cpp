#include "Precompile.h"
#include "scenes/LoadingScene.h"

#include "AppBuilder.h"
#include "SceneNavigator.h"
#include "Simulation.h"
#include "SceneList.h"
#include "loader/AssetReader.h"

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

  size_t requestTextureLoad(Row<TextureLoadRequest>& textures, ITableModifier& textureModifier, const char* filename) {
    const size_t i = textureModifier.addElements(1);
    TextureLoadRequest& request = textures.at(i);
    request.mFileName = filename;
    request.mImageID = std::hash<std::string>()(request.mFileName);
    return request.mImageID;
  }

  void initRequestAssets(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("request assets");
    SceneState* sceneState = task.query<SharedRow<SceneState>>().tryGetSingletonElement();
    FileSystem* fs = task.query<SharedRow<FileSystem>>().tryGetSingletonElement();
    if(!sceneState || !fs) {
      task.discard();
      return;
    }
    auto textureRequests = task.query<Row<TextureLoadRequest>>();
    std::shared_ptr<ITableModifier> textureRequestModifier = task.getModifierForTable(textureRequests.matchingTableIDs.front());
    auto playerTextures = task.query<SharedRow<TextureReference>, const IsPlayer>();
    auto fragmentTextures = task.query<SharedRow<TextureReference>, const IsFragment>();
    auto terrain = task.query<SharedRow<TextureReference>, const Tags::TerrainRow>();
    LoadingSceneGlobals* globals = getLoadingSceneGlobals(task);

    task.setCallback([=](AppTaskArgs&) mutable {
      loadLog("Loading...");
      //TODO: less hacky way to queue the initial request, probably make the caller figure out the necessary assets
      if(!globals->currentRequest.doInitialLoad) {
        return;
      }
      const std::string& root = fs->mRoot;
      sceneState->mBackgroundImage = requestTextureLoad(textureRequests.get<0>(0), *textureRequestModifier, (root + "background.png").c_str());
      sceneState->mPlayerImage = requestTextureLoad(textureRequests.get<0>(0), *textureRequestModifier, (root + "player.png").c_str());
      sceneState->mGroundImage = requestTextureLoad(textureRequests.get<0>(0), *textureRequestModifier, (root + "ground.png").c_str());

      for(size_t i = 0; i < playerTextures.size(); ++i) {
        playerTextures.get<0>(i).at().mId = sceneState->mPlayerImage;
      }
      //Make all the objects use the background image as their texture
      for(size_t i = 0; i < fragmentTextures.size(); ++i) {
        fragmentTextures.get<0>(i).at().mId = sceneState->mBackgroundImage;
      }
      for(size_t i = 0; i < terrain.size(); ++i) {
        terrain.get<0>(i).at().mId = sceneState->mGroundImage;
      }
    });
    builder.submitTask(std::move(task));
  }

  void awaitAssetLoading(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("Await Assets");
    auto textureRequests = task.query<const Row<TextureLoadRequest>>();
    auto requestModifiers = task.getModifiersForTables(textureRequests.matchingTableIDs);
    SceneState* sceneState = task.query<SharedRow<SceneState>>().tryGetSingletonElement();
    LoadingSceneGlobals* globals = getLoadingSceneGlobals(task);
    auto assetReader = Loader::createAssetReader(task);
    auto nav = SceneList::createNavigator(task);
    if(!sceneState) {
      task.discard();
      return;
    }

    task.setCallback([textureRequests, requestModifiers, sceneState, nav, assetReader, globals](AppTaskArgs&) mutable {
      loadLog(".");

      for(size_t i = 0; i < textureRequests.size(); ++i) {
        for(const TextureLoadRequest& request : textureRequests.get<0>(i).mElements) {
          switch(request.mStatus) {
            case RequestStatus::InProgress:
              //If any requests are pending, keep waiting
              return;
            case RequestStatus::Failed:
              printf("failed to load texture %s", request.mFileName.c_str());
              continue;
            case RequestStatus::Succeeded:
              continue;
          }
        }
      }

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

      //If they're all done, clear them and continue on to the next phase
      for(auto&& modifier : requestModifiers) {
        modifier->resize(0);
      }
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
      , loadingScene{ SceneList::get(task)->loading }
    {
    }

    void awaitLoadRequest(LoadRequest&& request) final {
      //Store the request so the loading screen knows what to load
      globals->currentRequest = std::move(request);
      //Go to the loading screen
      navigator->navigateTo(loadingScene);
    }

    LoadingSceneGlobals* globals{};
    std::shared_ptr<SceneNavigator::INavigator> navigator;
    SceneNavigator::SceneID loadingScene{};
  };

  std::shared_ptr<ILoadingNavigator> createLoadingNavigator(RuntimeDatabaseTaskBuilder& task) {
    return std::make_shared<LoadingNavigator>(task);
  }

  std::unique_ptr<IDatabase> createLoadingSceneDB(StableElementMappings& mappings) {
    return DBReflect::createDatabase<LoadingSceneDB>(mappings);
  }
}