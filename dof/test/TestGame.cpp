#include "Precompile.h"
#include "TestGame.h"

#include "AppBuilder.h"
#include "GameDatabase.h"
#include "GameBuilder.h"
#include "GameScheduler.h"
#include "TableAdapters.h"
#include "GameInput.h"
#include "scenes/SceneList.h"
#include "SceneNavigator.h"
#include "Physics.h"
#include "IGame.h"
#include "Game.h"

namespace Test {
  void addPassthroughMappings(Input::InputMapper& mapper) {
    mapper.addPassthroughAxis2D(GameInput::Keys::MOVE_2D);
  }

  struct InjectArgs : IAppModule {
    InjectArgs(Test::GameConstructArgs gcArgs)
      : args{ std::move(gcArgs) }
    {
    }

    void init(IAppBuilder& builder) {
      auto task = builder.createTask();
      //Disable loading from config
      task.query<SharedRow<FileSystem>>().tryGetSingletonElement()->mRoot = "?invalid?";
      Config::GameConfig* gameConfig = TableAdapters::getGameConfigMutable(task);

      task.setCallback([=](AppTaskArgs&) {
        //TODO: why is this assigned twice instead of assigning physics as part of GameConfig?
        *gameConfig = std::move(args.config);
        gameConfig->physics = std::move(args.physics);
      });
      builder.submitTask(std::move(task.setName("a")));
    }

    //Inject scene in dependentInit so it overrides the default scene started by SceneList module
    void dependentInit(IAppBuilder& builder) {
      if(!args.scene || builder.getEnv().isThreadLocal()) {
        return;
      }
      auto task = builder.createTask();
      auto myScene = SceneNavigator::createRegistry(task)->registerScene(std::move(args.scene));
      auto navigator = SceneNavigator::createNavigator(task);

      task.setCallback([=](AppTaskArgs&) {
        navigator->navigateTo(myScene);
      });

      builder.submitTask(std::move(task.setName("a")));
    };


    Test::GameConstructArgs args;
  };

  KnownTables::KnownTables(IAppBuilder& builder)
    : player{ builder.queryTables<IsPlayer>()[0] }
    , fragments{ builder.queryTables<IsFragment, SharedMassObjectTableTag>()[0] }
    , completedFragments{ builder.queryTables<FragmentGoalFoundTableTag>()[0] }
  {}

  TestGame::TestGame(GameConstructArgs args) {
    Game::GameArgs gameArgs = GameDefaults::createDefaultGameArgs(args.updateConfig);
    gameArgs.modules.insert(gameArgs.modules.begin(), std::make_unique<InjectArgs>(std::move(args)));
    game = Game::createGame(std::move(gameArgs));

    game->init();

    testBuilder = GameBuilder::create(game->getDatabase(), { AppEnvType::UpdateMain });
    taskArgs = game->createAppTaskArgs(0);

    auto temp = testBuilder->createTask();
    temp.discard();
    test = std::make_unique<RuntimeDatabaseTaskBuilder>(std::move(temp));
    test->discard();
    //TODO: args.updateConfig for Simulation?

    tables = KnownTables{ *testBuilder };

    addPassthroughMappings(*builder().query<GameInput::GlobalMappingsRow>().tryGetSingletonElement());
  }

  TestGame::TestGame(const GameArgs& args)
    : TestGame() {
    init(args);
  }

  GameConstructArgs toArgs(std::unique_ptr<SceneNavigator::IScene> scene) {
    GameConstructArgs result;
    result.scene = std::move(scene);
    return result;
  }

  TestGame::TestGame(std::unique_ptr<SceneNavigator::IScene> scene)
    : TestGame(toArgs(std::move(scene))) {
    //Update once to run events which will populate the broadphase
    update();
  }

  TestGame::~TestGame() = default;

  RuntimeDatabaseTaskBuilder& TestGame::builder() {
    return *test;
  }

  AppTaskArgs& TestGame::sharedArgs() {
    return *taskArgs;
  }

  void TestGame::init(const GameArgs& args) {
    auto b = builder();

    Config::FragmentConfig& fragment = TableAdapters::getGameConfigMutable(b)->fragment;
    if(!args.enableFragmentGoals) {
      fragment.fragmentGoalDistance = -1.0f;
    }
    if(args.forcedPadding) {
      TableAdapters::getGameConfigMutable(b)->physics.mForcedTargetWidth = *args.forcedPadding;
    }
    SceneState* scene = b.query<SharedRow<SceneState>>().tryGetSingletonElement();
    fragment.fragmentRows = args.fragmentCount + args.completedFragmentCount;
    fragment.fragmentColumns = 1;
    fragment.completedFragments = args.completedFragmentCount;
    fragment.playerSpawn = args.playerPos;
    fragment.addGround = false;
    auto nav = SceneList::createNavigator(b);
    nav.navigator->navigateTo(nav.scenes->fragment);

    //Needed for Test.cpp tests written before Z was introduced
    b.query<AccelerationZ>().forEachRow([](auto& v) { v.setDefaultValue(0.0f); });

    //Update once to run events which will populate the broadphase
    //I have accidentally introduced delays multiple times that I don't know the source of but am not concerned about it at the moment.
    update();
    update();
    update();
    update();

    scene->mBoundaryMin = glm::vec2(-100);
    scene->mBoundaryMax = glm::vec2(100);
  }


  void TestGame::update() {
    game->updateSimulation();
  }

  void TestGame::execute(std::unique_ptr<IAppBuilder> toExecute) {
    ThreadLocalsInstance* tls = builder().query<ThreadLocalsRow>().tryGetSingletonElement();
    execute(GameScheduler::buildTasks(IAppBuilder::finalize(std::move(toExecute)), *tls->instance));
  }

  void TestGame::execute(TaskRange range) {
    Scheduler* scheduler = builder().query<SharedRow<Scheduler>>().tryGetSingletonElement();
    range.mBegin->mTask.addToPipe(scheduler->mScheduler);
    scheduler->mScheduler.WaitforTask(range.mEnd->mTask.get());
  }

  ElementRef TestGame::getFromTable(const TableID& id, size_t index) {
    return builder().query<StableIDRow>(id).get<0>(0).at(index);
  }
}