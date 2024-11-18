#include "Precompile.h"
#include "scenes/SceneList.h"

#include "AppBuilder.h"
#include "SceneNavigator.h"
#include "scenes/EmptyScene.h"
#include "scenes/FragmentScene.h"
#include "scenes/ImportedScene.h"
#include "scenes/LoadingScene.h"
#include "scenes/PerformanceScenes.h"

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
    scenes->singleStack = registry->registerScene(::Scenes::createSingleStack());
    scenes->imported = registry->registerScene(::Scenes::createImportedScene());

    //Default start on fragment scene
    SceneNavigator::createNavigator(temp)->navigateTo(scenes->loading);
  }
};