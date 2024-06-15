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

namespace Test {
  void addPassthroughMappings(Input::InputMapper& mapper) {
    mapper.addPassthroughAxis2D(GameInput::Keys::MOVE_2D);
  }

  KnownTables::KnownTables(IAppBuilder& builder)
    : player{ builder.queryTables<IsPlayer>().matchingTableIDs[0] }
    , fragments{ builder.queryTables<IsFragment, SharedMassObjectTableTag>().matchingTableIDs[0] }
    , completedFragments{ builder.queryTables<FragmentGoalFoundTableTag>().matchingTableIDs[0] }
  {}

  TestGame::TestGame(GameConstructArgs args) {
    auto mappings = std::make_unique<StableElementMappings>();
    std::unique_ptr<IDatabase> game = GameDatabase::create(*mappings);
    std::unique_ptr<IDatabase> result =  DBReflect::bundle(std::move(game), std::move(mappings));

    std::unique_ptr<IAppBuilder> bootstrap = GameBuilder::create(*result);
    Simulation::initScheduler(*bootstrap);
    for(auto&& work : GameScheduler::buildSync(IAppBuilder::finalize(std::move(bootstrap)))) {
      work.work();
    }

    std::unique_ptr<IAppBuilder> initBuilder = GameBuilder::create(*result);
    auto temp = initBuilder->createTask();
    temp.discard();
    ThreadLocalsInstance* tls = temp.query<ThreadLocalsRow>().tryGetSingletonElement();

    Simulation::init(*initBuilder);
    TaskRange initTasks = GameScheduler::buildTasks(IAppBuilder::finalize(std::move(initBuilder)), *tls->instance);

    //Disable loading from config
    temp.query<SharedRow<FileSystem>>().tryGetSingletonElement()->mRoot = "?invalid?";
    Config::GameConfig* gameConfig = TableAdapters::getGameConfigMutable(temp);
    //TODO: why is this assigned twice instead of assigning physics as part of GameConfig?
    *gameConfig = std::move(args.config);
    gameConfig->physics = std::move(args.physics);

    Scheduler* scheduler = temp.query<SharedRow<Scheduler>>().tryGetSingletonElement();
    initTasks.mBegin->mTask.addToPipe(scheduler->mScheduler);
    scheduler->mScheduler.WaitforTask(initTasks.mEnd->mTask.get());

    testBuilder = GameBuilder::create(*result);

    std::unique_ptr<IAppBuilder> updateBuilder = GameBuilder::create(*result);
    if(args.scene) {
      auto t = updateBuilder->createTask();
      t.discard();
      auto myScene = SceneNavigator::createRegistry(t)->registerScene(std::move(args.scene));
      Simulation::buildUpdateTasks(*updateBuilder, args.updateConfig);
      SceneNavigator::createNavigator(t)->navigateTo(myScene);
    }
    else {
      Simulation::buildUpdateTasks(*updateBuilder, args.updateConfig);
    }
    GameInput::update(*updateBuilder);

    task = GameScheduler::buildTasks(IAppBuilder::finalize(std::move(updateBuilder)), *tls->instance);
    db = std::move(result);
    tables = KnownTables{ *testBuilder };
    test = std::make_unique<RuntimeDatabaseTaskBuilder>(std::move(temp));
    test->discard();
    tld = tls->instance->get(0);

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

  AppTaskArgs TestGame::sharedArgs() {
    AppTaskArgs result;
    result.threadLocal = &tld;
    return result;
  }

  void TestGame::init(const GameArgs& args) {
    auto a = sharedArgs();
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
    b.query<AccelerationZ>().forEachRow([](auto& v) { v.mDefaultValue = 0.0f; });

    //Update once to run events which will populate the broadphase
    update();
    update();
    update();

    scene->mBoundaryMin = glm::vec2(-100);
    scene->mBoundaryMax = glm::vec2(100);
  }


  void TestGame::update() {
    execute(task);
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
}