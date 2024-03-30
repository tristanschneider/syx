#include "Precompile.h"
#include "scenes/EmptyScene.h"

#include "AppBuilder.h"
#include "SceneNavigator.h"
#include "Simulation.h"
#include "SceneList.h"

namespace Scenes {
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

    task.setCallback([=](AppTaskArgs&) mutable {
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
    auto nav = SceneList::createNavigator(task);
    if(!sceneState) {
      task.discard();
      return;
    }

    task.setCallback([textureRequests, requestModifiers, sceneState, nav](AppTaskArgs&) mutable {
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

      //If they're all done, clear them and continue on to the next phase
      for(auto&& modifier : requestModifiers) {
        modifier->resize(0);
      }
      nav.navigator->navigateTo(nav.scenes->fragment);
    });
    builder.submitTask(std::move(task));
  }

  struct LoadingScene : SceneNavigator::IScene {
    void init(IAppBuilder& builder) final {
      initRequestAssets(builder);
    }
    void update(IAppBuilder& builder) final {
      awaitAssetLoading(builder);
    }
    void uninit(IAppBuilder&) final {}
  };

  std::unique_ptr<SceneNavigator::IScene> createLoadingScene() {
    return std::make_unique<LoadingScene>();
  }
}