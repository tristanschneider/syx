#include "Precompile.h"
#include "SceneNavigator.h"

#include "AppBuilder.h"
#include "RuntimeDatabase.h"
#include "DBEvents.h"

namespace SceneNavigator {
  constexpr size_t INVALID_SCENE = 0;

  enum class SceneState {
    NeedsInit,
    Updating,
    NeedsUninit
  };
  struct RegisteredScene {
    std::unique_ptr<IScene> scene;
  };
  struct Globals {
    std::unordered_map<SceneID, RegisteredScene> scenes;
    SceneState sceneState{ SceneState::Updating };
    bool hasRegisteredTasks{};
    SceneID transitioningTo{ INVALID_SCENE };
  };
  struct NavigatorData {
    SceneID current{ INVALID_SCENE };
    SceneID requested{ INVALID_SCENE };
  };
  struct GlobalsRow : SharedRow<Globals> {};
  struct NavigatorRow : SharedRow<NavigatorData> {};
  using SceneDB = Database<
    Table<
      GlobalsRow,
      NavigatorRow
    >
  >;

  std::shared_ptr<INavigator> createNavigator(RuntimeDatabaseTaskBuilder& task) {
    struct Nav : INavigator {
      Nav(RuntimeDatabaseTaskBuilder& task)
        : data{ task.query<NavigatorRow>().tryGetSingletonElement<0>() } {
      }

      SceneID getCurrentScene() const final {
        return data->current;
      }
      void navigateTo(SceneID scene) final {
        data->requested = scene;
      }

      NavigatorData* data{};
    };
    return std::make_shared<Nav>(task);
  }

  std::shared_ptr<ISceneRegistry> createRegistry(RuntimeDatabaseTaskBuilder& task) {
    struct Reg : ISceneRegistry {
      Reg(RuntimeDatabaseTaskBuilder& task)
        : globals{ task.query<GlobalsRow>().tryGetSingletonElement<0>() } {
      }

      SceneID registerScene(std::unique_ptr<IScene> scene) final {
        assert(!globals->hasRegisteredTasks && "Scene registration only supported before tasks are created");
        const SceneID result = globals->scenes.size() + 1;
        globals->scenes[result] = { std::move(scene) };
        return result;
      }

      Globals* globals{};
    };
    return std::make_shared<Reg>(task);
  }

  void createDB(RuntimeDatabaseArgs& args) {
    DBReflect::addDatabase<SceneDB>(args);
  }

  //Wraps the scene task in a callback that will first check if this scene should be performing the desired operation
  //This way the tasks can be written as if they were unconditional yet will only run when they should
  struct IntermediateBuilder : public IAppBuilder {
    IntermediateBuilder(IAppBuilder& builder)
      : parent{ builder } {
    }

    RuntimeDatabaseTaskBuilder createTask() override {
      return parent.createTask();
    }
    void submitTask(AppTaskWithMetadata&& task) override {
      //Query the additional data using a temporary task, manually appending the dependency information to the original
      auto temp = parent.createTask();
      auto query = temp.query<const NavigatorRow, const GlobalsRow>();
      auto nav = query.tryGetSingletonElement<0>();
      auto globals = query.tryGetSingletonElement<1>();
      temp.discard();
      task.data.append(std::move(temp).finalize().data);

      //Wrap the task so it's only called if on the current scene in the correct state
      AppTaskCallback originalCallback = std::move(task.task.callback);
      task.task.callback = [cb{ std::move(originalCallback) }, nav, globals, reqScene{ requiredScene }, reqState{ requiredState }](AppTaskArgs& args) {
        if(reqScene == nav->current && reqState == globals->sceneState) {
          cb(args);
        }
      };
      return parent.submitTask(std::move(task));
    }

    std::shared_ptr<AppTaskNode> finalize()&& override {
      return std::move(parent).finalize();
    }

    SceneID requiredScene{};
    SceneState requiredState{};
    IAppBuilder& parent;
  };

  void updateSceneState(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("Update Scene State");
    auto query = task.query<NavigatorRow, GlobalsRow>();
    auto nav = query.tryGetSingletonElement<0>();
    auto globals = query.tryGetSingletonElement<1>();

    task.setCallback([nav, globals](AppTaskArgs&) {
      switch(globals->sceneState) {
        case SceneState::NeedsInit:
          //Init time has elapsed, transition to update
          globals->sceneState = SceneState::Updating;
          break;
        case SceneState::Updating: {
          //While updating, check to see if any new transitions are requested to a different scene
          if(nav->requested != INVALID_SCENE) {
            globals->transitioningTo = nav->requested;
            nav->requested = INVALID_SCENE;
            globals->sceneState = SceneState::NeedsUninit;
          }
          break;
        }
        case SceneState::NeedsUninit: {
          //Uninit requested by scene transition has completed, init the newly requested scene
          nav->current = globals->transitioningTo;
          globals->transitioningTo = INVALID_SCENE;
          globals->sceneState = SceneState::NeedsInit;
          break;
        }
      }
    });

    builder.submitTask(std::move(task));
  }

  void defaultCleanup(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("Default Scene Cleanup");
    auto query = task.query<const IsClearedWithSceneTag, const StableIDRow>();
    auto globals = task.query<const GlobalsRow>().tryGetSingletonElement<0>();

    task.setCallback([globals, query](AppTaskArgs& args) mutable {
      if(globals->sceneState == SceneState::NeedsUninit) {
        Events::DestroyPublisher remove{ &args };

        for(size_t t = 0; t < query.size(); ++t) {
          auto [_, stable] = query.get(t);
          for(const auto& s : stable->mElements) {
            remove(s);
          }
        }
      }
    });

    builder.submitTask(std::move(task));
  }

  void update(IAppBuilder& builder) {
    auto temp = builder.createTask();
    temp.discard();
    Globals* globals = temp.query<GlobalsRow>().tryGetSingletonElement();
    globals->hasRegisteredTasks = true;

    updateSceneState(builder);

    IntermediateBuilder wrapper{ builder };
    for(auto& [id, scene] : globals->scenes) {
      wrapper.requiredScene = id;
      wrapper.requiredState = SceneState::NeedsInit;
      scene.scene->init(wrapper);
      wrapper.requiredState = SceneState::Updating;
      scene.scene->update(wrapper);
      wrapper.requiredState = SceneState::NeedsUninit;
      scene.scene->uninit(wrapper);
    }

    defaultCleanup(builder);
  }
}