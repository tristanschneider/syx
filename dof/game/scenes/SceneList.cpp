#include "Precompile.h"
#include "scenes/SceneList.h"

#include "AppBuilder.h"
#include "SceneNavigator.h"
#include "scenes/EmptyScene.h"
#include "scenes/FragmentScene.h"
#include "scenes/ImportedScene.h"
#include "scenes/LoadingScene.h"
#include "scenes/PerformanceScenes.h"
#include "IAppModule.h"

namespace SceneList {
  const Scenes* get(RuntimeDatabaseTaskBuilder& task) {
    return task.query<const ScenesRow>().tryGetSingletonElement();
  }

  ListNavigator createNavigator(RuntimeDatabaseTaskBuilder& task) {
    return { get(task), SceneNavigator::createNavigator(task) };
  }

  struct SceneListModule : IAppModule {
    void init(IAppBuilder& builder) final {
      if(builder.getEnv().isThreadLocal()) {
        return;
      }

      auto task = builder.createTask();
      auto registry = SceneNavigator::createRegistry(task);
      auto navigator = ::Scenes::createLoadingNavigator(task);
      Scenes* scenes = task.query<ScenesRow>().tryGetSingletonElement();

      task.setCallback([=](AppTaskArgs&) {
        scenes->empty = registry->registerScene(::Scenes::createEmptyScene());
        scenes->fragment = registry->registerScene(::Scenes::createFragmentScene());
        scenes->loading = registry->registerScene(::Scenes::createLoadingScene());
        scenes->singleStack = registry->registerScene(::Scenes::createSingleStack());
        scenes->imported = registry->registerScene(::Scenes::createImportedScene());

        //Default start on fragment scene
        navigator->awaitLoadRequest(::Scenes::LoadRequest{
          .onSuccess = scenes->fragment,
          .onFailure = scenes->fragment,
          .doInitialLoad = true
        });
      });

      builder.submitTask(std::move(task.setName("sceneListInit")));
    }
  };

  std::unique_ptr<IAppModule> createModule() {
    return std::make_unique<SceneListModule>();
  }
};