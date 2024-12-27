#pragma once

#include <loader/AssetLoader.h>

struct IDatabase;
struct RuntimeDatabaseArgs;

namespace SceneNavigator {
  struct IScene;
  using SceneID = size_t;
}

namespace Scenes {
  //Request awaiting the loading of these assets then transition to a scene on success or failure
  struct LoadRequest {
    std::vector<Loader::AssetHandle> toAwait;
    SceneNavigator::SceneID onSuccess{};
    SceneNavigator::SceneID onFailure{};
    //Request initial startup loading of base game assets
    bool doInitialLoad{};
  };

  struct ILoadingNavigator {
    virtual ~ILoadingNavigator() = default;
    virtual void awaitLoadRequest(LoadRequest&& request) = 0;
  };

  std::shared_ptr<ILoadingNavigator> createLoadingNavigator(RuntimeDatabaseTaskBuilder& task);

  std::unique_ptr<SceneNavigator::IScene> createLoadingScene();

  void createLoadingSceneDB(RuntimeDatabaseArgs& args);
}