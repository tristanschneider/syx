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
#include "TLSTaskImpl.h"

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
      Scenes* scenes = task.query<ScenesRow>().tryGetSingletonElement();

      task.setCallback([=](AppTaskArgs&) {
        scenes->empty = registry->registerScene(::Scenes::createEmptyScene());
        scenes->fragment = registry->registerScene(::Scenes::createFragmentScene());
        scenes->loading = registry->registerScene(::Scenes::createLoadingScene());
        scenes->singleStack = registry->registerScene(::Scenes::createSingleStack());
        scenes->imported = registry->registerScene(::Scenes::createImportedScene());
      });

      builder.submitTask(std::move(task.setName("sceneListInit")));
    }
  };

  std::unique_ptr<IAppModule> createModule() {
    return std::make_unique<SceneListModule>();
  }

  struct StartingSceneModule : IAppModule {
    struct Task {
      Task(RuntimeDatabaseTaskBuilder& task)
        : scenes{ task.query<const ScenesRow>().tryGetSingletonElement() }
        , navigator{ ::Scenes::createLoadingNavigator(task) }
      {
      }

      void execute(AppTaskArgs&) {
        //Default start on fragment scene
        navigator->awaitLoadRequest(::Scenes::LoadRequest{
          .onSuccess = scenes->fragment,
          .onFailure = scenes->fragment,
          .doInitialLoad = true
        });
      }

      const Scenes* scenes{};
      std::shared_ptr<::Scenes::ILoadingNavigator> navigator;
    };

    void init(IAppBuilder& builder) final {
      if(!builder.getEnv().isThreadLocal()) {
        builder.submitTask(TLSTask::create<Task>("default scene"));
      }
    }
  };

  std::unique_ptr<IAppModule> createStartingSceneModule() {
    return std::make_unique<StartingSceneModule>();
  }
};