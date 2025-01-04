#include "Precompile.h"
#include "Game.h"

#include "IGame.h"

#include "AppBuilder.h"
#include "GameBuilder.h"
#include "GameScheduler.h"
#include "Scheduler.h"
#include "ThreadLocals.h"

namespace Game {
  GameArgs::GameArgs() = default;
  GameArgs::GameArgs(GameArgs&&) = default;
  GameArgs::~GameArgs() = default;

  struct TaskGraphItem {
    TaskRange mt;
    std::vector<GameScheduler::SyncWorkItem> st;
  };
  struct TaskGraph {
    TaskGraphItem update;
    TaskGraphItem renderCommit;
  };

  TaskGraphItem createTaskGraphItem(std::shared_ptr<AppTaskNode> appTaskNodes, ThreadLocals* tls) {
    if(tls) {
      return TaskGraphItem{
        .mt = GameScheduler::buildTasks(std::move(appTaskNodes), *tls),
      };
    }
    return TaskGraphItem{
      .st = GameScheduler::buildSync(std::move(appTaskNodes)),
    };
  }

  TaskGraphItem createTaskGraphItem(std::unique_ptr<IAppBuilder> builder, ThreadLocals* tls) {
    return createTaskGraphItem(IAppBuilder::finalize(std::move(builder)), tls);
  }

  void runTask(TaskGraphItem& task, Scheduler* mt) {
    if(mt) {
      task.mt.mBegin->mTask.addToPipe(mt->mScheduler);
      mt->mScheduler.WaitforTask(task.mt.mEnd->mTask.get());
    }
    else {
      for(const auto& t : task.st) {
        t.work();
      }
    }
  }

  void assertEqual(RuntimeDatabase& original, RuntimeDatabase& copy) {
    assert(original.size() == copy.size());
    for(size_t i = 0; i < original.size(); ++i) {
      assert(original[i].getType() == copy[i].getType());
      assert(original[i].getID() == copy[i].getID());
    }
  }

  class GameImpl : public IGame {
  public:
    GameImpl(GameArgs&& a)
      : args{ std::move(a) }
    {
    }

    ThreadLocalDatabaseFactory getThreadLocalDatabaseFactory(IDatabase& main) {
      //Create a copy of the main database with the same stable mappings as the main one.
      //The copy should have all the same table ids so that they can be used to prepare desired elements
      //to be migrated from the thread local database to the main one
      return [&] {
        RuntimeDatabase& runtimeMain = main.getRuntime();
        StableElementMappings& mappings = runtimeMain.getMappings();
        std::unique_ptr<IDatabase> copy = createDatabase(RuntimeDatabaseArgs{ .mappings = &mappings });
        assertEqual(runtimeMain, copy->getRuntime());
        return copy;
      };
    }

    std::unique_ptr<IDatabase> createDatabase(RuntimeDatabaseArgs&& dbArgs) {
      visitModules(dbArgs, &IAppModule::createDatabase);
      visitModules(dbArgs, &IAppModule::createDependentDatabase);
      return std::make_unique<RuntimeDatabase>(std::move(dbArgs));
    }

    void buildAndRunInit(std::unique_ptr<IAppBuilder> builder) {
      visitModules(*builder, &IAppModule::init);
    }

    void init() final {
      //Create database
      db = createDatabase(DBReflect::createArgsWithMappings());

      //Initialize scheduler if available
      {
        const ThreadLocalDatabaseFactory factory = getThreadLocalDatabaseFactory(*db);
        std::unique_ptr<IAppBuilder> bootstrap = GameBuilder::create(*db, { AppEnvType::InitScheduler });
        for(auto& m : args.modules) {
          m->initScheduler(*bootstrap, factory);
        }
        TaskGraphItem initTask = createTaskGraphItem(std::move(bootstrap), nullptr);
        runTask(initTask, nullptr);
      }
      //May be empty if no module initialized it
      threading = args.dbSource->getMultithreadedDeps(*db);

      //Init modules
      buildAndRunInit(GameBuilder::create(*db, { AppEnvType::InitMain }));
      //Init thread local databases
      if(threading) {
        for(size_t i = 0; i < threading.tls->getThreadCount(); ++i) {
          if(RuntimeDatabase* localDB = threading.tls->get(i).statEffects) {
            buildAndRunInit(GameBuilder::create(*threading.tls->get(i).statEffects, { AppEnvType::InitThreadLocal }));
          }
        }
      }

      graph = buildUpdate();
    }

    void updateRendering() final {
      runTask(graph.renderCommit, threading.scheduler);
    }

    void updateSimulation() final {
      runTask(graph.update, threading.scheduler);
    }

    IDatabase& getDatabase() final {
      return *db;
    }

    TaskGraph buildUpdate() {
      std::unique_ptr<IAppBuilder> builder = GameBuilder::create(getDatabase(), { AppEnvType::UpdateMain });

      if(args.rendering) {
        args.rendering->preSimUpdate(*builder);
      }
      visitModules(*builder, &IAppModule::update);

      visitModules(*builder, &IAppModule::preProcessEvents);
      visitModules(*builder, &IAppModule::processEvents);
      visitModules(*builder, &IAppModule::postProcessEvents);

      if(args.rendering) {
        args.rendering->postSimUpdate(*builder);
      }

      std::shared_ptr<AppTaskNode> appTaskNodes = IAppBuilder::finalize(std::move(builder));
      //TODO: put this back if I care
      //constexpr bool outputGraph = false;
      //if(outputGraph && appTaskNodes) {
      //  GraphViz::writeHere("graph.gv", *appTaskNodes);
      //}

      return TaskGraph{
        .update = createTaskGraphItem(std::move(appTaskNodes), threading.tls),
        .renderCommit = buildRenderUpdate(),
      };
    }

    TaskGraphItem buildRenderUpdate() {
      std::unique_ptr<IAppBuilder> commitBuilder = GameBuilder::create(getDatabase(), { AppEnvType::UpdateMain });
      if(args.rendering) {
        args.rendering->renderOnlyUpdate(*commitBuilder);
      }
      return createTaskGraphItem(std::move(commitBuilder), threading.tls);
    }

    template<class T>
    void visitModules(T& builder, void(IAppModule::*fn)(T&)) {
      visitModules([&](IAppModule& m) {
        (m.*fn)(builder);
      });
    }

    void visitModules(const std::function<void(IAppModule&)>& visitor) {
      if(args.rendering) {
        visitor(*args.rendering);
      }
      for(auto& m : args.modules) {
        visitor(*m);
      }
    }

    GameArgs args;
    std::unique_ptr<IDatabase> db;
    TaskGraph graph;
    MultithreadedDeps threading;
  };

  std::unique_ptr<IGame> createGame(GameArgs&& args) {
    return std::make_unique<GameImpl>(std::move(args));
  }
}

#include "Simulation.h"
#include "GameInput.h"

namespace GameDefaults {
  MultithreadedDeps DefaultGameDatabaseReader::getMultithreadedDeps(IDatabase& db) {
    RuntimeDatabase& rdb = db.getRuntime();
    ThreadLocalsInstance* instance = rdb.query<ThreadLocalsRow>().tryGetSingletonElement();
    Scheduler* scheduler = rdb.query<SharedRow<Scheduler>>().tryGetSingletonElement();

    return MultithreadedDeps{
      .tls = instance ? instance->instance.get() : nullptr,
      .scheduler = scheduler,
    };
  }

  Game::GameArgs createDefaultGameArgs() {
    Game::GameArgs args;
    args.dbSource = std::make_unique<DefaultGameDatabaseReader>();
    args.modules.push_back(Simulation::createModule({}));
    //Rendering is from a separate project so "default" exposed here is empty
    return args;
  }
}