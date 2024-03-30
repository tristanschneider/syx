#include "Precompile.h"
#include "scenes/SceneList.h"

#include "AppBuilder.h"
#include "SceneNavigator.h"
#include "scenes/EmptyScene.h"
#include "scenes/FragmentScene.h"
#include "scenes/LoadingScene.h"

namespace SceneList {
  const Scenes* get(RuntimeDatabaseTaskBuilder& task) {
    return task.query<const ScenesRow>().tryGetSingletonElement();
  }

  ListNavigator createNavigator(RuntimeDatabaseTaskBuilder& task) {
    return { get(task), SceneNavigator::createNavigator(task) };
  }

  void registerScenes(IAppBuilder& builder) {
    auto temp = builder.createTask();
    temp.discard();
    auto registry = SceneNavigator::createRegistry(temp);
    Scenes* scenes = temp.query<ScenesRow>().tryGetSingletonElement();
    scenes->empty = registry->registerScene(::Scenes::createEmptyScene());
    scenes->fragment = registry->registerScene(::Scenes::createFragmentScene());
    scenes->loading = registry->registerScene(::Scenes::createLoadingScene());

    //Default start on fragment scene
    SceneNavigator::createNavigator(temp)->navigateTo(scenes->loading);
  }
};