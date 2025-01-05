#include "Precompile.h"
#include "TestApp.h"

#include "GameBuilder.h"
#include "Simulation.h"
#include "IAppModule.h"
#include "IGame.h"
#include "Game.h"

namespace Test {
  TestApp::TestApp() = default;
  TestApp::~TestApp() = default;

  struct InjectArgsModule : IAppModule {
    InjectArgsModule(TestApp::InitArgs&& ia)
      : injectArgs{ std::move(ia) }
    {
    }

    void createDatabase(RuntimeDatabaseArgs& args) final {
      if(injectArgs.buildDB) {
        injectArgs.buildDB(args);
      }
    }

    //Add a scheduler if it was requested and missing
    void createDependentDatabase(RuntimeDatabaseArgs& args) final {
      if(!injectArgs.initScheduler) {
        return;
      }
      bool hasThreadLocals = false;
      for(const RuntimeTableRowBuilder& table : args.tables) {
        if(table.contains<ThreadLocalsRow>()) {
          hasThreadLocals = true;
          break;
        }
      }
      if(!hasThreadLocals) {
        DBReflect::addDatabase<Database<Table<ThreadLocalsRow, SharedRow<Scheduler>, Events::EventsRow>>>(args);
      }
    }

    //Single threaded initialization of the scheduler itself. Minimize work here.
    void initScheduler(IAppBuilder& builder, const ThreadLocalDatabaseFactory& f) final {
      if(injectArgs.initScheduler) {
        Simulation::initScheduler(builder, f);
      }
    }

    void update(IAppBuilder& builder) final {
      if(injectArgs.buildWork) {
        injectArgs.buildWork(builder);
      }
    }

    TestApp::InitArgs injectArgs;
  };

  std::unique_ptr<RuntimeDatabaseTaskBuilder> createTaskBuilder(IGame& game) {
    auto builder = GameBuilder::create(game.getDatabase(), { AppEnvType::UpdateMain });
    auto temp = builder->createTask();
    temp.discard();
    auto result = std::make_unique<RuntimeDatabaseTaskBuilder>(std::move(temp));
    result->discard();
    return result;
  }

  void TestApp::initImpl(const InitArgs& initArgs) {
    Game::GameArgs gameArgs;
    gameArgs.modules.push_back(std::make_unique<InjectArgsModule>(InitArgs{ initArgs }));
    game = Game::createGame(std::move(gameArgs));
    taskBuilder = createTaskBuilder(*game);
  }

  void TestApp::initST(const DBBuilder& buildDB, const WorkBuilder& buildWork) {
    initImpl(InitArgs{
      .buildDB = buildDB,
      .buildWork = buildWork,
      .initScheduler = false
    });
  }

  void TestApp::initMT(const DBBuilder& buildDB, const WorkBuilder& buildWork) {
    initImpl(InitArgs{
      .buildDB = buildDB,
      .buildWork = buildWork,
      .initScheduler = true
    });
  }

  RuntimeDatabaseTaskBuilder& TestApp::builder() {
    return *taskBuilder;
  }

  void TestApp::update() {
    game->updateSimulation();
  }

  ElementRef TestApp::createInTable(const TableID& table) {
    return builder().query<StableIDRow>(table).get<0>(0).at(builder().getModifierForTable(table)->addElements(1));
  }
}