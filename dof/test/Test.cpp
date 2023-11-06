#include "Precompile.h"
#include "CppUnitTest.h"

#include "config/ConfigIO.h"
#include "stat/AllStatEffects.h"
#include "Physics.h"
#include "Simulation.h"
#include "SweepNPrune.h"
#include "Table.h"
#include "TableOperations.h"
#include "Queries.h"

#include "PhysicsSimulation.h"
#include "Scheduler.h"
#include "StableElementID.h"
#include "TableAdapters.h"
#include "ThreadLocals.h"
#include "Fragment.h"
#include "Player.h"
#include "DBEvents.h"

#include "GameDatabase.h"
#include "GameBuilder.h"
#include "GameScheduler.h"
#include "RuntimeDatabase.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Test {
  using namespace Config;
  //TODO: get rid of duple 
  static_assert(std::is_same_v<Duple<int*>, Table<Row<int>>::ElementRef>);
  static_assert(std::is_same_v<Table<Row<int>>::ElementRef, decltype(make_duple((int*)nullptr))>);
  static_assert(std::is_same_v<std::tuple<DupleElement<0, int>>, Duple<int>::TupleT>);

  struct GameArgs {
    size_t fragmentCount{};
    size_t completedFragmentCount{};
    //Optionally create a player, and if so, at this position
    std::optional<glm::vec2> playerPos;
    //Boundary of broadphase and forces. Default should be enough to keep it out of the way
    glm::vec2 boundaryMin{ -100 };
    glm::vec2 boundaryMax{ 100 };
    bool enableFragmentGoals{ false };
    std::optional<size_t> forcedPadding;
  };

  //Hack for some funky ordering, these are for construct, GameArgs are for init
  struct GameConstructArgs {
    Config::GameConfig config;
    Config::PhysicsConfig physics;
    Simulation::UpdateConfig updateConfig;
  };

  struct KnownTables {
    KnownTables() = default;
    KnownTables(IAppBuilder& builder)
      : player{ builder.queryTables<IsPlayer>().matchingTableIDs[0] }
      , fragments{ builder.queryTables<IsFragment, SharedMassObjectTableTag>().matchingTableIDs[0] }
      , completedFragments{ builder.queryTables<FragmentGoalFoundTableTag>().matchingTableIDs[0] }
    {}
    UnpackedDatabaseElementID player;
    UnpackedDatabaseElementID fragments;
    UnpackedDatabaseElementID completedFragments;
  };

  struct TestGame {
    TestGame(GameConstructArgs args = {}) {
      auto mappings = std::make_unique<StableElementMappings>();
      std::unique_ptr<IDatabase> game = GameData::create(*mappings);
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
      Simulation::buildUpdateTasks(*updateBuilder, args.updateConfig);

      task = GameScheduler::buildTasks(IAppBuilder::finalize(std::move(updateBuilder)), *tls->instance);
      db = std::move(result);
      tables = KnownTables{ *testBuilder };
      test = std::make_unique<RuntimeDatabaseTaskBuilder>(std::move(temp));
      test->discard();
      tld = tls->instance->get(0);
    }

    TestGame(const GameArgs& args)
      : TestGame() {
      init(args);
    }

    RuntimeDatabaseTaskBuilder& builder() {
      return *test;
    }

    AppTaskArgs sharedArgs() {
      AppTaskArgs result;
      result.threadLocal = &tld;
      return result;
    }

    void init(const GameArgs& args) {
      auto a = sharedArgs();
      auto b = builder();

      if(!args.enableFragmentGoals) {
        TableAdapters::getGameConfigMutable(b)->fragment.fragmentGoalDistance = -1.0f;
      }
      if(args.forcedPadding) {
        TableAdapters::getGameConfigMutable(b)->physics.mForcedTargetWidth = *args.forcedPadding;
      }

      b.getModifierForTable(tables.fragments)->resize(args.fragmentCount);
      b.getModifierForTable(tables.completedFragments)->resize(args.completedFragmentCount);
      StableIDRow* stableRow = &b.query<StableIDRow>(tables.fragments).get<0>(0);
      for(size_t i = 0; i < args.fragmentCount; ++i) {
        Events::onNewElement(StableElementID::fromStableRow(i, *stableRow), a);
      }
      stableRow = &b.query<StableIDRow>(tables.completedFragments).get<0>(0);
      for(size_t i = 0; i < args.completedFragmentCount; ++i) {
        Events::onNewElement(StableElementID::fromStableRow(i, *stableRow), a);
      }

      SceneState* scene = b.query<SharedRow<SceneState>>().tryGetSingletonElement();
      scene->mState = SceneState::State::Update;
      scene->mBoundaryMin = glm::vec2(-100);
      scene->mBoundaryMax = glm::vec2(100);

      if(args.playerPos) {
        createPlayer(*args.playerPos);
      }
      //Update once to run events which will populate the broadphase
      update();
    }

    void createPlayer(const glm::vec2& pos) {
      auto b = builder();
      auto a = sharedArgs();
      b.getModifierForTable(tables.player)->resize(1);
      StableIDRow* stableIds = &b.query<StableIDRow>(tables.player).get<0>(0);
      Events::onNewElement(StableElementID::fromStableRow(0, *stableIds), a);

      auto&& [px, py, rx, ry] = b.query<FloatRow<Tags::Pos, Tags::X>, FloatRow<Tags::Pos, Tags::Y>,
        FloatRow<Tags::Rot, Tags::CosAngle>, FloatRow<Tags::Rot, Tags::SinAngle>>(tables.player).get(0);

      TableAdapters::write(0, pos, *px, *py);
      TableAdapters::write(0, { 1, 0 }, *rx, *ry);
    }

    void update() {
      execute(task);
    }

    void execute(std::unique_ptr<IAppBuilder> toExecute) {
      ThreadLocalsInstance* tls = builder().query<ThreadLocalsRow>().tryGetSingletonElement();
      execute(GameScheduler::buildTasks(IAppBuilder::finalize(std::move(toExecute)), *tls->instance));
    }

    void execute(TaskRange range) {
      Scheduler* scheduler = builder().query<SharedRow<Scheduler>>().tryGetSingletonElement();
      range.mBegin->mTask.addToPipe(scheduler->mScheduler);
      scheduler->mScheduler.WaitforTask(range.mEnd->mTask.get());
    }

    TaskRange task;
    std::unique_ptr<IDatabase> db;
    std::unique_ptr<IAppBuilder> testBuilder;
    std::unique_ptr<RuntimeDatabaseTaskBuilder> test;
    KnownTables tables;
    ThreadLocalData tld;
  };

  TEST_CLASS(GameSchedulerTest) {
    struct TestDB {
      struct TestDBT : Database<
        Table<Row<int>, Row<float>, StableIDRow>,
        Table<Row<int64_t>, StableIDRow>
      > {};

      TestDB() {
        db = createDB();
        scheduler.mScheduler.Initialize();
        tls = std::make_unique<ThreadLocals>(scheduler.mScheduler.GetNumTaskThreads(), events.impl.get(), &db->getRuntime().getMappings());
        builder = GameBuilder::create(*db);
      }

      static std::unique_ptr<IDatabase> createDB() {
        auto mappings = std::make_unique<StableElementMappings>();
        auto db = DBReflect::createDatabase<TestDBT>(*mappings);
        return DBReflect::bundle(std::move(db), std::move(mappings));
      }

      TaskRange build() {
        auto result = GameScheduler::buildTasks(IAppBuilder::finalize(std::move(builder)), *tls);
        //Replace with a new one
        builder = GameBuilder::create(*db);
        return result;
      }

      void execute(TaskRange task) {
        task.mBegin->mTask.addToPipe(scheduler.mScheduler);
        scheduler.mScheduler.WaitforTask(task.mEnd->mTask.get());
      }

      void buildAndExecute() {
        execute(build());
      }

      std::unique_ptr<IDatabase> db;
      std::unique_ptr<ThreadLocals> tls;
      Events::EventsInstance events;
      Scheduler scheduler;
      std::unique_ptr<IAppBuilder> builder;
    };

    TEST_METHOD(ConfigurableTask) {
      TestDB db;
      auto taskA = db.builder->createTask();
      auto taskB = db.builder->createTask();
      auto taskC = db.builder->createTask();
      std::shared_ptr<AppTaskConfig> configB = taskB.getConfig();
      std::shared_ptr<AppTaskConfig> configC = taskC.getConfig();
      //Write in a read in b so scheduler sequences them
      taskA.query<Row<int>>();
      taskB.query<const Row<int>>();
      taskC.query<const Row<int>>();
      taskA.setName("a").setCallback([configB, configC](...) {
        AppTaskSize s;
        s.batchSize = 5;
        s.workItemCount = 0;
        configB->setSize(s);
        s.workItemCount = 2;
        configC->setSize(s);
      });
      taskB.setName("b").setCallback([](AppTaskArgs&) {
        Assert::Fail(L"Zero size tasks should not be invoked");
      });
      int cInvocations{};
      taskC.setName("c").setCallback([&cInvocations](AppTaskArgs& args) {
        ++cInvocations;
        Assert::AreEqual(size_t(0), args.begin);
        Assert::AreEqual(size_t(2), args.end);
      });
      db.builder->submitTask(std::move(taskA));
      db.builder->submitTask(std::move(taskB));
      db.builder->submitTask(std::move(taskC));

      db.buildAndExecute();

      Assert::AreEqual(1, cInvocations);
    }

    TEST_METHOD(MigrateTable) {
      TestDB db;
      const UnpackedDatabaseElementID from = db.builder->queryTables<Row<float>>().matchingTableIDs[0];
      const UnpackedDatabaseElementID to = db.builder->queryTables<Row<int64_t>>().matchingTableIDs[0];
      int invocations{};
      {
        auto taskA = db.builder->createTask();
        auto modifier = taskA.getModifierForTable(from);
        taskA.setName("a").setCallback([modifier, &invocations](...) {
          ++invocations;
          modifier->resize(2);
        });
        db.builder->submitTask(std::move(taskA));
      }
      {
        auto taskB = db.builder->createTask();
        RuntimeDatabase& d = taskB.getDatabase();
        taskB.setName("b").setCallback([&d, from, to, &invocations](...) {
          ++invocations;
          RuntimeTable* tableFrom = d.tryGet(from);
          RuntimeTable* tableTo = d.tryGet(to);
          Assert::IsTrue(tableFrom && tableTo);

          RuntimeTable::migrateOne(0, *tableFrom, *tableTo);

          Assert::AreEqual(size_t(1), tableFrom->tryGet<Row<float>>()->size());
          Assert::AreEqual(size_t(1), tableTo->tryGet<Row<int64_t>>()->size());
        });
        db.builder->submitTask(std::move(taskB));
      }
      {
        auto taskC = db.builder->createTask();
        auto query = taskC.query<Row<int64_t>>();
        taskC.setName("c").setCallback([query, &invocations](...) mutable {
          ++invocations;
          Assert::AreEqual(size_t(1), query.get<0>(0).size());
        });
        db.builder->submitTask(std::move(taskC));
      }
      db.buildAndExecute();
      Assert::AreEqual(3, invocations);
    }
  };

  TEST_CLASS(Tests) {
    TEST_METHOD(Table_AddElement_SizeIncreases) {
      Table<Row<int>> table;

      auto tuple = TableOperations::addToTable(table);
      tuple.get<0>() = 5;

      Assert::AreEqual(size_t(1), TableOperations::size(table));
    }

    TEST_METHOD(TableWithElement_getElement_HasValue) {
      Table<Row<int>> table;
      TableOperations::addToTable(table).get<0>() = 6;

      int value = TableOperations::getElement(table, 0).get<0>();

      Assert::AreEqual(6, value);
    }

    struct IntRowA : Row<int> {};
    struct IntRowB : Row<int> {};

    TEST_METHOD(TwoRowsSameElementTypeDifferentValues_getRow_HasDifferentValues) {
      Table<IntRowA, IntRowB> table;
      auto tuple = TableOperations::addToTable(table);
      tuple.get<0>() = 1;
      tuple.get<1>() = 2;

      int rowA = TableOperations::getRow<IntRowA>(table).at(0);
      int rowB = TableOperations::getRow<IntRowB>(table).at(0);

      Assert::AreEqual(1, rowA);
      Assert::AreEqual(2, rowB);
    }

    TEST_METHOD(TableWithElement_swapRemove_IsRemoved) {
      Table<Row<int>> table;
      TableOperations::addToTable(table);

      TableOperations::swapRemove(table, 0);

      Assert::AreEqual(size_t(0), TableOperations::size(table));
    }

    TEST_METHOD(TableWithRow_hasRow_True) {
      Table<Row<int>> table;

      Assert::IsTrue(TableOperations::hasRow<Row<int>>(table));
    }

    TEST_METHOD(TableWithoutRow_hasRow_False) {
      Table<Row<int>> table;

      Assert::IsFalse(TableOperations::hasRow<Row<std::string>>(table));
    }

    TEST_METHOD(SameTables_migrateOne_ValuesMoved) {
      Table<Row<int>> a, b;
      TableOperations::addToTable(a).get<0>() = 5;

      TableOperations::migrateOne(a, b, 0);

      Assert::AreEqual(5, TableOperations::getElement(b, 0).get<0>());
    }

    TEST_METHOD(SameTables_migrateAll_ValuesMoved) {
      Table<Row<int>> a, b;
      TableOperations::addToTable(a).get<0>() = 5;

      TableOperations::migrateAll(a, b);

      Assert::AreEqual(5, TableOperations::getElement(b, 0).get<0>());
    }

    TEST_METHOD(SmallAndBigTable_MigrateOneSmallToBig_ValuesMoved) {
      Table<Row<int>> a;
      Table<Row<int>, Row<short>> b;
      TableOperations::addToTable(a).get<0>() = 5;

      TableOperations::migrateOne(a, b, 0);

      Assert::AreEqual(5, TableOperations::getElement(b, 0).get<0>());
      Assert::AreEqual(size_t(1), TableOperations::getRow<Row<short>>(b).size());
    }

    TEST_METHOD(SmallAndBigTable_MigrateAllSmallToBig_ValuesMoved) {
      Table<Row<int>> a;
      Table<Row<int>, Row<short>> b;
      TableOperations::addToTable(a).get<0>() = 5;

      TableOperations::migrateAll(a, b);

      Assert::AreEqual(5, TableOperations::getElement(b, 0).get<0>());
      Assert::AreEqual(size_t(1), TableOperations::getRow<Row<short>>(b).size());
    }

    TEST_METHOD(SmallAndBigTable_MigrateOneBigToSmall_ValuesMoved) {
      Table<Row<int>> a;
      Table<Row<int>, Row<short>> b;
      TableOperations::addToTable(b).get<0>() = 5;

      TableOperations::migrateOne(b, a, 0);

      Assert::AreEqual(5, TableOperations::getElement(a, 0).get<0>());
    }

    TEST_METHOD(SmallAndBigTable_MigrateAllBigToSmall_ValuesMoved) {
      Table<Row<int>> a;
      Table<Row<int>, Row<short>> b;
      TableOperations::addToTable(b).get<0>() = 5;

      TableOperations::migrateAll(b, a);

      Assert::AreEqual(5, TableOperations::getElement(a, 0).get<0>());
    }

    TEST_METHOD(Tables_Query_AreFound) {
      Database<
        Table<Row<int>>,
        Table<Row<int>, Row<short>>,
        Table<Row<uint64_t>>
      > db;
      TableOperations::addToTable(std::get<0>(db.mTables)).get<0>() = 1;
      TableOperations::addToTable(std::get<1>(db.mTables)).get<0>() = 2;
      TableOperations::addToTable(std::get<2>(db.mTables)).get<0>() = uint64_t(3);

      int total = 0;
      Queries::viewEachRow<Row<int>>(db, [&](Row<int>& row) {
        total += row.at(0);
      });

      Assert::AreEqual(3, total);
    }

    TEST_METHOD(TableWithSharedRow_GetValue_IsSameForAll) {
      Table<Row<int>, SharedRow<int>> table;
      std::get<1>(table.mRows).at() = 5;
      TableOperations::addToTable(table);
      TableOperations::addToTable(table);

      const int a = std::get<1>(table.mRows).at(0);
      const int b = std::get<1>(table.mRows).at(0);

      Assert::AreEqual(5, a);
      Assert::AreEqual(5, b);
    }

    TEST_METHOD(DatabaseElement_Construct_HasPackedValues) {
      DatabaseElementID<3> id(2, 7);

      Assert::AreEqual(size_t(2), id.getTableIndex());
      Assert::AreEqual(size_t(7), id.getElementIndex());
    }

    TEST_METHOD(ZeroElementID_Unpack_IsValid) {
      DatabaseElementID<4> id(0, 0);

      Assert::IsTrue(id.isValid());
      Assert::AreEqual(size_t(0), id.getTableIndex());
      Assert::AreEqual(size_t(0), id.getElementIndex());
    }

    TEST_METHOD(EmptyElementID_IsValid_False) {
      Assert::IsFalse(DatabaseElementID<2>{}.isValid());
    }

    TEST_METHOD(TwoRows_AddToSortedTable_IsSorted) {
      Table<Row<int>, Row<size_t>, SharedRow<bool>> table;
      TableOperations::addToSortedTable<Row<int>>(table, int(5)).get<1>() = size_t(5);
      TableOperations::addToSortedTable<Row<int>>(table, int(4)).get<1>() = size_t(4);
      TableOperations::addToSortedTable<Row<int>>(table, int(6)).get<1>() = size_t(6);

      Assert::IsTrue(std::vector<int>{ 4, 5, 6 } == std::get<Row<int>>(table.mRows).mElements);
      Assert::IsTrue(std::vector<size_t>{ size_t(4), size_t(5), size_t(6) } == std::get<Row<size_t>>(table.mRows).mElements);
    }

    TEST_METHOD(SortedTable_SortedRemove_IsStillSorted) {
      Table<Row<int>, Row<size_t>, SharedRow<bool>> table;
      TableOperations::addToSortedTable<Row<int>>(table, int(5)).get<1>() = size_t(5);
      TableOperations::addToSortedTable<Row<int>>(table, int(4)).get<1>() = size_t(4);
      TableOperations::addToSortedTable<Row<int>>(table, int(6)).get<1>() = size_t(6);

      TableOperations::sortedRemove(table, size_t(1));

      Assert::IsTrue(std::vector<int>{ 4, 6 } == std::get<Row<int>>(table.mRows).mElements);
      Assert::IsTrue(std::vector<size_t>{ size_t(4), size_t(6) } == std::get<Row<size_t>>(table.mRows).mElements);
    }

    TEST_METHOD(SortedUniqueTable_AddDuplicate_IsNotAdded) {
      Table<Row<int>> table;
      TableOperations::addToSortedUniqueTable<Row<int>>(table, 1);
      TableOperations::addToSortedUniqueTable<Row<int>>(table, 1);

      Assert::AreEqual(size_t(1), TableOperations::size(table));
    }

    TEST_METHOD(SortedTable_AddDuplicate_IsAdded) {
      Table<Row<int>> table;
      TableOperations::addToSortedTable<Row<int>>(table, 1);
      TableOperations::addToSortedTable<Row<int>>(table, 1);

      Assert::AreEqual(size_t(2), TableOperations::size(table));
    }

    using TestDB = Database<Table<Row<int>>, Table<Row<size_t>>>;
    static_assert(TestDB::ElementID(1, 0) == TestDB::getTableIndex<Table<Row<size_t>>>());

    TEST_METHOD(TwoTables_VisitByIndex_IsVisited) {
      struct TestTable : Table<Row<int>> {};
      struct TestTable2 : TestTable {};
      Database<TestTable, TestTable2> database;
      TableOperations::addToTable(std::get<TestTable2>(database.mTables));

      bool wasVisited = false;
      database.visitOneByIndex(database.getTableIndex<TestTable>(), [&](auto& table) {
        wasVisited = true;
        Assert::AreEqual((void*)(&std::get<TestTable>(database.mTables)), (void*)(&table));
      });

      Assert::IsTrue(wasVisited);
    }

    TEST_METHOD(TwoTables_GetRowInTable_IsReturned) {
      struct TestTable : Table<Row<int>> {};
      struct TestTable2 : TestTable {};
      Database<TestTable, TestTable2> database;

      Row<int>* row = Queries::getRowInTable<Row<int>>(database, database.getTableIndex<TestTable2>());

      Assert::IsTrue(&std::get<Row<int>>(std::get<TestTable2>(database.mTables).mRows) == row);
    }

    static void _execute(TaskRange task) {
      if(task.mBegin) {
        enki::TaskScheduler scheduler;
        scheduler.Initialize();
        TaskRange toRun = TaskBuilder::buildDependencies(task.mBegin);
        toRun.mBegin->mTask.addToPipe(scheduler);
        scheduler.WaitforAll();
      }
    }

    static StableElementMappings& _getStableMappings(GameDatabase& db) {
      return std::get<SharedRow<StableElementMappings>>(std::get<GlobalGameData>(db.mTables).mRows).at();
    }

    static PhysicsConfig _configWithPadding(size_t padding) {
      PhysicsConfig result;
      result.mForcedTargetWidth = padding;
      return result;
    }

    static void _buildConstraintsTable(TestGame& game) {
      std::unique_ptr<IAppBuilder> temp = GameBuilder::create(*game.db);
      auto b = game.builder();
      const Config::PhysicsConfig* config = TableAdapters::getPhysicsConfig(game.builder());
      ConstraintsTableBuilder::build(*temp, *config);
      game.execute(std::move(temp));
    }

    TEST_METHOD(CollidingPair_PopulateNarrowphase_IsPopulated) {
      TestGame game;
      GameArgs args;
      args.fragmentCount = 2;
      game.init(args);

      TransformAdapter transform = TableAdapters::getTransform(game.builder(), game.tables.fragments);
      transform.posX->at(0) = 1.1f;
      transform.posX->at(1) = 1.2f;
      auto& narrowphasePosX = game.builder().query<NarrowphaseData<PairA>::PosX>().get<0>(0);

      game.update();

      Assert::AreEqual(size_t(1), narrowphasePosX.size());
      //Don't really care what order it copied in as long as the values were copied from gameobjects to narrowphase
      Assert::IsTrue(narrowphasePosX.at(0) == 1.1f || narrowphasePosX.at(0) == 1.2f);
    }

    TEST_METHOD(DistantPair_PopulateNarrowphase_NoPairs) {
      TestGame game;
      GameArgs args;
      args.fragmentCount = 2;
      game.init(args);

      auto transform = TableAdapters::getTransform(game.builder(), game.tables.fragments);
      auto& pairs = game.builder().query<NarrowphaseData<PairA>::PosX>().get<0>(0);
      transform.posX->at(0) = 1.0f;
      transform.posX->at(1) = 5.0f;

      game.update();

      Assert::AreEqual(size_t(0), pairs.size());
    }

    TEST_METHOD(TwoPairsSameObject_GenerateCollisionPairs_HasPairs) {
      TestGame game;
      GameArgs args;
      args.fragmentCount = 3;
      game.init(args);


      auto transform = TableAdapters::getTransform(game.builder(), game.tables.fragments);
      auto& posX = *transform.posX;
      auto&& [pairA, pairB] = game.builder().query<CollisionPairIndexA, CollisionPairIndexB>().get(0);
      //This one to collide with both
      posX.at(0) = 5.0f;
      //This one to the left to collide with 1 but not 2
      posX.at(1) = 4.0f;
      //To the right, colliding with 0 but not 1
      posX.at(2) = 6.0f + SweepNPruneBroadphase::BoundariesConfig{}.mPadding;

      game.update();

      Assert::AreEqual(size_t(2), pairA->size());
      std::pair<size_t, size_t> a, b;
      //These asserts are enforcing a result order but that's not a requirement, rather it's easier to write the test that way
      const UnpackedDatabaseElementID table = game.tables.fragments;
      Assert::IsTrue(StableElementID{ table.remakeElement(0).mValue, 1 } == pairA->at(0));
      Assert::IsTrue(StableElementID{ table.remakeElement(1).mValue, 2 } == pairB->at(0));

      Assert::IsTrue(StableElementID{ table.remakeElement(0).mValue, 1 } == pairA->at(1));
      Assert::IsTrue(StableElementID{ table.remakeElement(2).mValue, 3 } == pairB->at(1));
    }

    TEST_METHOD(CollidingPair_GenerateContacts_AreGenerated) {
      TestGame game;
      GameArgs args;
      args.fragmentCount = 2;
      game.init(args);
      TransformAdapter transform = TableAdapters::getTransform(game.builder(), game.tables.fragments);
      auto& posX = *transform.posX;
      auto&& [contactOverlap, contactX, contactNormal] = game.builder().query<
        ContactPoint<ContactOne>::Overlap,
        ContactPoint<ContactOne>::PosX,
        SharedNormal::X
      >().get(0);
      const float expectedOverlap = 0.1f;
      posX.at(0) = 5.0f;
      posX.at(1) = 6.0f - expectedOverlap;

      game.update();

      Assert::AreEqual(size_t(1), contactOverlap->size());
      const float e = 0.00001f;
      Assert::AreEqual(expectedOverlap, contactOverlap->at(0), e);
      Assert::AreEqual(5.5f - expectedOverlap, contactX->at(0), e);
      Assert::AreEqual(-1.0f, contactNormal->at(0), e);
    }

    TEST_METHOD(CollidingPair_SolveConstraints_AreSeparated) {
      TestGame game;
      GameArgs args;
      args.fragmentCount = 2;
      game.init(args);
      auto transform = TableAdapters::getTransform(game.builder(), game.tables.fragments);
      auto physics = TableAdapters::getPhysics(game.builder(), game.tables.fragments);
      const float expectedOverlap = 0.1f;
      transform.posX->at(0) = 5.0f;
      physics.linVelX->at(0) = 1.0f;
      transform.posX->at(1) = 6.0f - expectedOverlap;
      physics.linVelX->at(1) = -1.0f;

      game.update();

      const glm::vec2 centerAToContact{ 0.4f, 0.5f };
      const glm::vec2 centerBToContact{ -0.5f, 0.5f };
      const glm::vec2 normal{ -1.0f, 0.0f };
      //Velocity of contact point as velocity of center plus the angular component which is angular velocity cross the center to contact projected onto the x axis
      const float xVelocityOfAAtContactA = physics.linVelX->at(0) - physics.angVel->at(0)*centerAToContact.y;
      const float xVelocityOfBAtContactA = physics.linVelX->at(1) - physics.angVel->at(1)*centerBToContact.y;
      const float velocityDifference = xVelocityOfAAtContactA + xVelocityOfBAtContactA;

      //Need to be pretty loose on the comparison because friction makes the rotation part not completely zero
      const float e = 0.01f;
      //Constraint is trying to solve for the difference of the projection of the velocities of the contact point on A and B on the normal being zero
      Assert::AreEqual(0.0f, velocityDifference, e);
    }

    struct SweepEntry {
      glm::vec2 mNewBoundaryMin{};
      glm::vec2 mNewBoundaryMax{};
      size_t mKey{};
      Broadphase::BroadphaseKey broadphaseKey;
    };

    static void _assertSorted(const Broadphase::Sweep2D& sweep) {
      std::array<std::vector<float>, Broadphase::Sweep2D::S> values;
      for(size_t d = 0; d < Broadphase::Sweep2D::S; ++d) {
        for(size_t i = 0; i < sweep.axis[d].elements.size(); ++i) {
          const Broadphase::SweepElement& k = sweep.axis[d].elements[i];
          const auto& pair = sweep.bounds[d][k.getValue()];
          values[d].push_back(k.isStart() ? pair.first : pair.second);
        }
      }

      for(size_t d = 0; d < Broadphase::Sweep2D::S; ++d) {
        Assert::IsTrue(std::is_sorted(values[d].begin(), values[d].end()), L"All elements should be sorted by bounds");
        std::vector<Broadphase::SweepElement> elements = sweep.axis[d].elements;
        std::sort(elements.begin(), elements.end());
        Assert::IsTrue(std::unique(elements.begin(), elements.end()) == elements.end(), L"There should be no duplicate elements");
      }
    }

    static void _clear(std::vector<Broadphase::SweepCollisionPair>& a, std::vector<Broadphase::SweepCollisionPair>& b) {
      a.clear();
      b.clear();
    }

    static void _insertOne(Broadphase::Sweep2D& sweep, SweepEntry& entry, std::vector<Broadphase::SweepCollisionPair>& gained) {
      Broadphase::SweepNPrune::insertRange(sweep, &entry.mKey, &entry.broadphaseKey, size_t(1));
      Broadphase::SweepNPrune::updateBoundaries(sweep, &entry.mNewBoundaryMin.x, &entry.mNewBoundaryMax.x, &entry.mNewBoundaryMin.y, &entry.mNewBoundaryMax.y, &entry.broadphaseKey, 1);
      std::vector<Broadphase::SweepCollisionPair> lost;
      Broadphase::SwapLog log{ gained, lost };
      Broadphase::CollisionCandidates candidates;
      Broadphase::SweepNPrune::recomputePairs(sweep, candidates, log);
      Assert::IsTrue(lost.empty(), L"Nothing should be lost when inserting elements");
      _assertSorted(sweep);
    }

    static void _eraseOne(Broadphase::Sweep2D& sweep, SweepEntry& entry, std::vector<Broadphase::SweepCollisionPair>& lost) {
      Broadphase::SweepNPrune::eraseRange(sweep, &entry.broadphaseKey, 1);
      std::vector<Broadphase::SweepCollisionPair> gained;
      Broadphase::SwapLog log{ gained, lost };
      Broadphase::CollisionCandidates candidates;
      Broadphase::SweepNPrune::recomputePairs(sweep, candidates, log);
      Assert::IsTrue(gained.empty(), L"Nothing should be gained when removing an element");
      _assertSorted(sweep);
    }

    static void _reinsertOne(Broadphase::Sweep2D& sweep, SweepEntry& entry, std::vector<Broadphase::SweepCollisionPair>& gained, std::vector<Broadphase::SweepCollisionPair>& lost) {
      Broadphase::SweepNPrune::updateBoundaries(sweep, &entry.mNewBoundaryMin.x, &entry.mNewBoundaryMax.x, &entry.mNewBoundaryMin.y, &entry.mNewBoundaryMax.y, &entry.broadphaseKey, 1);
      Broadphase::SwapLog log{ gained, lost };
      Broadphase::CollisionCandidates candidates;
      Broadphase::SweepNPrune::recomputePairs(sweep, candidates, log);
      _assertSorted(sweep);
    }

    static bool pairMatches(Broadphase::SweepCollisionPair l, Broadphase::SweepCollisionPair r) {
      return r == l;
    }

    static void assertPairsMatch(const std::vector<Broadphase::SweepCollisionPair>& l, std::initializer_list<Broadphase::SweepCollisionPair> r) {
      Assert::AreEqual(l.size(), r.size());
      Assert::IsTrue(l == std::vector<Broadphase::SweepCollisionPair>{ r });
    }

    TEST_METHOD(SweepNPrune_EraseOneOverlappingAxis_NoLoss) {
      Broadphase::Sweep2D sweep;
      std::vector<Broadphase::SweepCollisionPair> pairs;
      SweepEntry entry;
      entry.mKey = size_t(1);
      entry.mNewBoundaryMin = glm::vec2(1.0f, 2.0f);
      entry.mNewBoundaryMax = glm::vec2(2.0f, 3.0f);
      _insertOne(sweep, entry, pairs);
      Assert::IsTrue(pairs.empty());

      //Move out of contact on one axis
      entry.mNewBoundaryMax.x += 5.0f;
      entry.mNewBoundaryMin.x += 5.0f;
      entry.mKey = size_t(2);
      _insertOne(sweep, entry, pairs);
      Assert::IsTrue(pairs.empty());

      _eraseOne(sweep, entry, pairs);

      Assert::IsTrue(pairs.empty());
    }

    static Broadphase::SweepCollisionPair sweepPair(size_t a, size_t b) {
      return Broadphase::SweepCollisionPair{ a, b };
    }

    TEST_METHOD(SweepNPrune_InsertRange) {
      Broadphase::Sweep2D sweep;
      std::vector<Broadphase::SweepCollisionPair> pairs;
      SweepEntry entry;
      entry.mKey = size_t(1);
      entry.mNewBoundaryMin = glm::vec2(1.0f, 2.0f);
      entry.mNewBoundaryMax = glm::vec2(2.0f, 3.0f);

      _insertOne(sweep, entry, pairs);
      Assert::IsTrue(pairs.empty());
      //Insert another at the same coordinates, should cause new pair
      SweepEntry same = entry;
      same.mKey = size_t(2);
      _insertOne(sweep, same, pairs);
      assertPairsMatch(pairs, { sweepPair(1, 2) });
      pairs.clear();

      //Insert one to the left of both of the previous ones
      SweepEntry left;
      left.mNewBoundaryMin = entry.mNewBoundaryMin - glm::vec2(2.0f);
      left.mNewBoundaryMax = left.mNewBoundaryMin + glm::vec2(1.0f);
      left.mKey = 3;
      _insertOne(sweep, left, pairs);
      assertPairsMatch(pairs, {});

      //Insert one to the right of all of the previous
      SweepEntry right;
      right.mNewBoundaryMin = entry.mNewBoundaryMax + glm::vec2(1.0f);
      right.mNewBoundaryMax = right.mNewBoundaryMin + glm::vec2(1.0f);
      right.mKey = 4;
      _insertOne(sweep, right, pairs);
      assertPairsMatch(pairs, {});

      //Insert on the boundary of left and center
      SweepEntry leftToCenter;
      leftToCenter.mNewBoundaryMin = left.mNewBoundaryMax;
      leftToCenter.mNewBoundaryMax = entry.mNewBoundaryMin;
      leftToCenter.mKey = 5;
      _insertOne(sweep, leftToCenter, pairs);
      assertPairsMatch(pairs, { sweepPair(5, 1), sweepPair(5, 2), sweepPair(5, 3) });
      pairs.clear();

      //Entirely containing right
      SweepEntry rightOverlap;
      rightOverlap.mNewBoundaryMin = right.mNewBoundaryMin - glm::vec2(0.1f);
      rightOverlap.mNewBoundaryMax = right.mNewBoundaryMax + glm::vec2(0.1f);
      rightOverlap.mKey = 6;
      _insertOne(sweep, rightOverlap, pairs);
      assertPairsMatch(pairs, { sweepPair(6, 4) });
      pairs.clear();

      //Contained by right and rightOVerlap
      SweepEntry rightContained;
      rightContained.mNewBoundaryMin = right.mNewBoundaryMin + glm::vec2(0.1f);
      rightContained.mNewBoundaryMax = rightContained.mNewBoundaryMin + glm::vec2(0.1f);
      rightContained.mKey = 7;
      _insertOne(sweep, rightContained, pairs);
      assertPairsMatch(pairs, { sweepPair(7, 4), sweepPair(7, 6) });
      pairs.clear();

      std::vector<Broadphase::SweepCollisionPair> lostPairs;
      _eraseOne(sweep, rightContained, lostPairs);
      assertPairsMatch(lostPairs, { sweepPair(7, 4), sweepPair(7, 6) });
      lostPairs.clear();

      _eraseOne(sweep, rightOverlap, lostPairs);
      assertPairsMatch(lostPairs, { sweepPair(6, 4) });
      lostPairs.clear();

      _eraseOne(sweep, leftToCenter, lostPairs);
      assertPairsMatch(lostPairs, { sweepPair(5, 1), sweepPair(5, 2), sweepPair(5, 3) });
      lostPairs.clear();

      _eraseOne(sweep, right, lostPairs);
      assertPairsMatch(lostPairs, {});
      lostPairs.clear();

      _eraseOne(sweep, left, lostPairs);
      assertPairsMatch(lostPairs, {});
      lostPairs.clear();

      _eraseOne(sweep, same, lostPairs);
      assertPairsMatch(lostPairs, { sweepPair(1, 2) });
      lostPairs.clear();

      _eraseOne(sweep, entry, lostPairs);
      assertPairsMatch(lostPairs, {});
      lostPairs.clear();
    }

    TEST_METHOD(SweepNPrune_ReinsertRange) {
      Broadphase::Sweep2D sweep;
      std::vector<Broadphase::SweepCollisionPair> gainedPairs, lostPairs;
      const float size = 1.0f;
      const float space = 0.1f;

      SweepEntry upperLeft;
      upperLeft.mKey = 1;
      upperLeft.mNewBoundaryMin = glm::vec2(0.0f, size + space);
      upperLeft.mNewBoundaryMax = upperLeft.mNewBoundaryMin + glm::vec2(size);

      SweepEntry upperRight;
      upperRight.mKey = 2;
      upperRight.mNewBoundaryMin = glm::vec2(size + space, size + space);
      upperRight.mNewBoundaryMax = upperRight.mNewBoundaryMin + glm::vec2(size);

      SweepEntry bottomLeft;
      bottomLeft.mKey = 3;
      bottomLeft.mNewBoundaryMin = glm::vec2(0.0f);
      bottomLeft.mNewBoundaryMax = bottomLeft.mNewBoundaryMin + glm::vec2(size);

      SweepEntry bottomRight;
      bottomRight.mKey = 4;
      bottomRight.mNewBoundaryMin = glm::vec2(size + space, 0.0f);
      bottomRight.mNewBoundaryMax = bottomRight.mNewBoundaryMin + glm::vec2(size);

      _insertOne(sweep, upperLeft, gainedPairs);
      _insertOne(sweep, upperRight, gainedPairs);
      _insertOne(sweep, bottomLeft, gainedPairs);
      _insertOne(sweep, bottomRight, gainedPairs);
      Assert::IsTrue(gainedPairs.empty());

      //Reinsert in place, nothing should happen
      _reinsertOne(sweep, upperLeft, gainedPairs, lostPairs);
      assertPairsMatch(gainedPairs, {});
      assertPairsMatch(lostPairs, {});

      //Put bottom left to the right of top right. This makes it lose and gain an axis at the same time relative to top left
      bottomLeft.mNewBoundaryMin = upperRight.mNewBoundaryMin + glm::vec2(size + space*2.0f, 0.0f);
      bottomLeft.mNewBoundaryMax = bottomLeft.mNewBoundaryMin + glm::vec2(size);
      _reinsertOne(sweep, bottomLeft, gainedPairs, lostPairs);
      assertPairsMatch(gainedPairs, {});
      assertPairsMatch(lostPairs, {});

      //Put bottom left to the left of upper left, so it has now completely switched sides on one axis relative to the two top entries
      bottomLeft.mNewBoundaryMin = upperLeft.mNewBoundaryMin - glm::vec2(size + space*2.0f, 0.0f);
      bottomLeft.mNewBoundaryMax = bottomLeft.mNewBoundaryMin + glm::vec2(size);
      _reinsertOne(sweep, bottomLeft, gainedPairs, lostPairs);
      assertPairsMatch(gainedPairs, {});
      assertPairsMatch(lostPairs, {});

      //Put it back
      bottomLeft.mNewBoundaryMin = glm::vec2(0.0f);
      bottomLeft.mNewBoundaryMax = bottomLeft.mNewBoundaryMin + glm::vec2(size);
      _reinsertOne(sweep, bottomLeft, gainedPairs, lostPairs);
      assertPairsMatch(gainedPairs, {});
      assertPairsMatch(lostPairs, {});

      //Extend upperRight down left to contain all others
      upperRight.mNewBoundaryMin = glm::vec2(-1.0f);
      _reinsertOne(sweep, upperRight, gainedPairs, lostPairs);
      assertPairsMatch(gainedPairs, { sweepPair(2, 1), sweepPair(2, 3), sweepPair(2, 4) });
      assertPairsMatch(lostPairs, {});
      _clear(gainedPairs, lostPairs);

      //Move bottom left away from all
      bottomLeft.mNewBoundaryMax -= glm::vec2(100.0f);
      bottomLeft.mNewBoundaryMin -= glm::vec2(100.0f);
      _reinsertOne(sweep, bottomLeft, gainedPairs, lostPairs);
      assertPairsMatch(gainedPairs, {});
      assertPairsMatch(lostPairs, { sweepPair(3, 2) });
      _clear(gainedPairs, lostPairs);

      //Undo the previous move
      bottomLeft.mNewBoundaryMax += glm::vec2(100.0f);
      bottomLeft.mNewBoundaryMin += glm::vec2(100.0f);
      _reinsertOne(sweep, bottomLeft, gainedPairs, lostPairs);
      assertPairsMatch(gainedPairs, { sweepPair(3, 2) });
      assertPairsMatch(lostPairs, {});
      _clear(gainedPairs, lostPairs);

      //Move right to within lower right, but out of the left two
      upperRight.mNewBoundaryMin.x = bottomRight.mNewBoundaryMin.x + 0.1f;
      _reinsertOne(sweep, upperRight, gainedPairs, lostPairs);
      assertPairsMatch(gainedPairs, {});
      assertPairsMatch(lostPairs, { sweepPair(2, 1), sweepPair(2, 3) });
      _clear(gainedPairs, lostPairs);

      //Restore right to how it started
      upperRight.mNewBoundaryMin = glm::vec2(size + space, size + space);
      upperRight.mNewBoundaryMax = upperRight.mNewBoundaryMin + glm::vec2(size);
      _reinsertOne(sweep, upperRight, gainedPairs, lostPairs);
      assertPairsMatch(gainedPairs, {});
      assertPairsMatch(lostPairs, { sweepPair(2, 4) });
      _clear(gainedPairs, lostPairs);

      //Extend bottom left up into upper right, overlapping with everything
      bottomLeft.mNewBoundaryMax += glm::vec2(size * 0.5f);
      _reinsertOne(sweep, bottomLeft, gainedPairs, lostPairs);
      assertPairsMatch(gainedPairs, { sweepPair(3, 1), sweepPair(3, 2), sweepPair(3, 4) });
      assertPairsMatch(lostPairs, {});
      _clear(gainedPairs, lostPairs);

      //Shrink it back on top so it's only overlapping with bottom right
      bottomLeft.mNewBoundaryMax.y = bottomRight.mNewBoundaryMax.y;
      _reinsertOne(sweep, bottomLeft, gainedPairs, lostPairs);
      assertPairsMatch(gainedPairs, {});
      assertPairsMatch(lostPairs, { sweepPair(3, 1), sweepPair(3, 2) });
      _clear(gainedPairs, lostPairs);

      //Resize and move bottom left to inside of upperRight
      bottomLeft.mNewBoundaryMin = upperRight.mNewBoundaryMin + glm::vec2(0.1f);
      bottomLeft.mNewBoundaryMax = bottomLeft.mNewBoundaryMax + glm::vec2(0.1f);
      _reinsertOne(sweep, bottomLeft, gainedPairs, lostPairs);
      assertPairsMatch(gainedPairs, { sweepPair(3, 2) });
      assertPairsMatch(lostPairs, { sweepPair(3, 4) });
    }

    using StableTableA = Table<
      Row<int>,
      StableIDRow
    >;
    using StableTableB = Table<
      Row<uint64_t>,
      Row<int>,
      StableIDRow
    >;
    using FillerTable = Table<Row<int>>;
    //Put a filler table at index 0 so the tests aren't testing with table index 0 which can easily cause false passes when the id isn't used.
    using TestStableDB = Database<FillerTable, StableTableA, StableTableB>;

    template<class TableT>
    static void verifyAllMappings(TestStableDB& db, TableT& table, StableElementMappings& mappings) {
      constexpr auto tableIndex = TestStableDB::getTableIndex<TableT>();
      auto& stableA = std::get<StableIDRow>(table.mRows);
      for(size_t i = 0; i < stableA.size(); ++i) {
        const size_t stable = stableA.at(i);
        const size_t unstable = TestStableDB::ElementID{ tableIndex.getTableIndex(), i }.mValue;
        Assert::AreEqual(mappings.findKey(stable)->second, unstable);
        StableElementID sid{ unstable, stable };
        std::optional<StableElementID> resolved = StableOperations::tryResolveStableID(sid, db, mappings);
        Assert::IsTrue(resolved.has_value());
        Assert::IsTrue(*resolved == sid);
      }
    }

    TEST_METHOD(StableElementID_Operations) {
      StableElementMappings mappings;
      TestStableDB db;
      auto& a = std::get<StableTableA>(db.mTables);
      auto& b = std::get<StableTableB>(db.mTables);
      auto& stableA = std::get<StableIDRow>(a.mRows);
      auto& valueA = std::get<Row<int>>(a.mRows);
      constexpr auto tableIndexA = UnpackedDatabaseElementID::fromPacked(TestStableDB::getTableIndex<StableTableA>());
      constexpr auto tableIndexB = UnpackedDatabaseElementID::fromPacked(TestStableDB::getTableIndex<StableTableB>());
      TableOperations::stableResizeTable(a, tableIndexA, 3, mappings);

      verifyAllMappings(db, a, mappings);

      valueA.at(0) = 1;
      valueA.at(2) = 5;
      StableElementID elementA = StableOperations::getStableID(stableA, tableIndexA);
      StableElementID elementC = StableOperations::getStableID(stableA, TestStableDB::ElementID{ tableIndexA.getTableIndex(), 2 });
      TableOperations::stableSwapRemove(a, TestStableDB::ElementID{ tableIndexA.getTableIndex(), 0 }, mappings);

      verifyAllMappings(db, a, mappings);

      Assert::AreEqual(5, valueA.at(0));
      Assert::IsFalse(StableOperations::tryResolveStableID(elementA, db, mappings).has_value());
      TestStableDB::ElementID resolvedC{ StableOperations::tryResolveStableID(elementC, db, mappings)->mUnstableIndex };
      Assert::AreEqual(5, Queries::getRowInTable<Row<int>>(db, resolvedC)->at(resolvedC.getElementIndex()));

      //Migrate object at index 0 in A which is ElementC
      TableOperations::stableMigrateOne(a, b, TestStableDB::getTableIndex<StableTableA>(), TestStableDB::getTableIndex<StableTableB>(), mappings);

      verifyAllMappings(db, a, mappings);
      verifyAllMappings(db, b, mappings);

      resolvedC = TestStableDB::ElementID{ StableOperations::tryResolveStableID(elementC, db, mappings)->mUnstableIndex };
      Assert::AreEqual(5, Queries::getRowInTable<Row<int>>(db, resolvedC)->at(resolvedC.getElementIndex()));

      TableOperations::stableResizeTable(a, tableIndexA, 0, mappings);
      TableOperations::stableResizeTable(b, tableIndexB, 0, mappings);

      Assert::IsTrue(mappings.empty());
    }
    /*
    struct DBReader {
      DBReader(GameDatabase& db)
        : mGameObjects(std::get<GameObjectTable>(db.mTables))
        , mStaticGameObjects(std::get<StaticGameObjectTable>(db.mTables))
        , mBroadphase(std::get<BroadphaseTable>(db.mTables))
        , mCollisionPairs(std::get<CollisionPairsTable>(db.mTables))
        , mConstraints(std::get<ConstraintsTable>(db.mTables))
        , mConstraintsCommon(std::get<ConstraintCommonTable>(db.mTables))
        , mStaticConstraints(std::get<ContactConstraintsToStaticObjectsTable>(db.mTables))
        , mPlayers(std::get<PlayerTable>(db.mTables))
        , mGlobals(std::get<GlobalGameData>(db.mTables))
        , mPosX(std::get<FloatRow<Tags::Pos, Tags::X>>(mGameObjects.mRows))
        , mStaticPosX(std::get<FloatRow<Tags::Pos, Tags::X>>(mStaticGameObjects.mRows))
        , mPlayerPosX(std::get<FloatRow<Tags::Pos, Tags::X>>(mPlayers.mRows))
        , mPosY(std::get<FloatRow<Tags::Pos, Tags::Y>>(mGameObjects.mRows))
        , mStaticPosY(std::get<FloatRow<Tags::Pos, Tags::Y>>(mStaticGameObjects.mRows))
        , mPlayerPosY(std::get<FloatRow<Tags::Pos, Tags::Y>>(mPlayers.mRows))
        , mLinVelX(std::get<FloatRow<Tags::LinVel, Tags::X>>(mGameObjects.mRows))
        , mLinVelY(std::get<FloatRow<Tags::LinVel, Tags::Y>>(mGameObjects.mRows))
        , mPlayerLinVelX(std::get<FloatRow<Tags::LinVel, Tags::X>>(mPlayers.mRows))
        , mPlayerLinVelY(std::get<FloatRow<Tags::LinVel, Tags::Y>>(mPlayers.mRows))
        , mGameObjectIDs(std::get<StableIDRow>(mGameObjects.mRows))
        , mStaticGameObjectIDs(std::get<StableIDRow>(mStaticGameObjects.mRows))
        , mPlayerIDs(std::get<StableIDRow>(mPlayers.mRows))
        , mCollisionPairIDs(std::get<StableIDRow>(mCollisionPairs.mRows))
        , mConstraintIDs(std::get<StableIDRow>(mConstraintsCommon.mRows))
        , mCollisionPairA(std::get<CollisionPairIndexA>(mCollisionPairs.mRows))
        , mCollisionPairB(std::get<CollisionPairIndexB>(mCollisionPairs.mRows))
        , mConstraintPairA(std::get<CollisionPairIndexA>(mConstraintsCommon.mRows))
        , mConstraintPairB(std::get<CollisionPairIndexB>(mConstraintsCommon.mRows))
        , mSyncIndexA(std::get<ConstraintObject<ConstraintObjA>::SyncIndex>(mConstraintsCommon.mRows))
        , mSyncIndexB(std::get<ConstraintObject<ConstraintObjB>::SyncIndex>(mConstraintsCommon.mRows))
        , mSyncTypeA(std::get<ConstraintObject<ConstraintObjA>::SyncType>(mConstraintsCommon.mRows))
        , mSyncTypeB(std::get<ConstraintObject<ConstraintObjB>::SyncType>(mConstraintsCommon.mRows))
        , mIsConstraintEnabled(std::get<ConstraintData::IsEnabled>(mConstraintsCommon.mRows))
        , mChangedPairs(std::get<SharedRow<SweepNPruneBroadphase::ChangedCollisionPairs>>(mBroadphase.mRows).at())
        , mConstraintsMappings(std::get<SharedRow<ConstraintsTableMappings>>(mGlobals.mRows).at())
        , mStableMappings(std::get<SharedRow<StableElementMappings>>(mGlobals.mRows).at())
        , mContactPairedConstraint(std::get<ConstraintElement>(mCollisionPairs.mRows))
      {
      }

      GameObjectTable& mGameObjects;
      StaticGameObjectTable& mStaticGameObjects;
      BroadphaseTable& mBroadphase;
      CollisionPairsTable& mCollisionPairs;
      ConstraintsTable& mConstraints;
      ConstraintCommonTable& mConstraintsCommon;
      ContactConstraintsToStaticObjectsTable& mStaticConstraints;
      PlayerTable& mPlayers;
      GlobalGameData& mGlobals;

      SweepNPruneBroadphase::ChangedCollisionPairs& mChangedPairs;

      FloatRow<Tags::Pos, Tags::X>& mPosX;
      FloatRow<Tags::Pos, Tags::X>& mStaticPosX;
      FloatRow<Tags::Pos, Tags::X>& mPlayerPosX;
      FloatRow<Tags::Pos, Tags::Y>& mPosY;
      FloatRow<Tags::Pos, Tags::Y>& mStaticPosY;
      FloatRow<Tags::Pos, Tags::Y>& mPlayerPosY;

      FloatRow<Tags::LinVel, Tags::X>& mLinVelX;
      FloatRow<Tags::LinVel, Tags::Y>& mLinVelY;
      FloatRow<Tags::LinVel, Tags::X>& mPlayerLinVelX;
      FloatRow<Tags::LinVel, Tags::Y>& mPlayerLinVelY;

      StableIDRow& mGameObjectIDs;
      StableIDRow& mStaticGameObjectIDs;
      StableIDRow& mPlayerIDs;
      StableIDRow& mCollisionPairIDs;
      StableIDRow& mConstraintIDs;
      StableElementMappings& mStableMappings;
      ConstraintsTableMappings& mConstraintsMappings;
      ConstraintData::IsEnabled& mIsConstraintEnabled;

      ConstraintElement& mContactPairedConstraint;

      ConstraintObject<ConstraintObjA>::SyncIndex& mSyncIndexA;
      ConstraintObject<ConstraintObjB>::SyncIndex& mSyncIndexB;
      ConstraintObject<ConstraintObjA>::SyncType& mSyncTypeA;
      ConstraintObject<ConstraintObjB>::SyncType& mSyncTypeB;

      CollisionPairIndexA& mCollisionPairA;
      CollisionPairIndexA& mConstraintPairA;
      CollisionPairIndexB& mCollisionPairB;
      CollisionPairIndexB& mConstraintPairB;
    };
    */

    static bool _unorderedIdsMatch(const CollisionPairIndexA& actualA, const CollisionPairIndexB& actualB,
      std::vector<StableElementID> expectedA,
      std::vector<StableElementID> expectedB,
      const ConstraintData::IsEnabled* isEnabled) {
      Assert::AreEqual(expectedA.size(), expectedB.size());

      for(size_t i = 0; i < actualA.size(); ++i) {
        if(isEnabled && !CollisionMask::shouldSolveConstraint(isEnabled->at(i))) {
          continue;
        }
        const StableElementID& toFindA = actualA.at(i);
        const StableElementID& toFindB = actualB.at(i);
        bool found = false;
        for(size_t j = 0; j < expectedA.size(); ++j) {
          if((toFindA == expectedA[j] && toFindB == expectedB[j]) ||
            (toFindA == expectedB[j] && toFindB == expectedA[j])) {
            found = true;
            expectedA.erase(expectedA.begin() + j);
            expectedB.erase(expectedB.begin() + j);
            break;
          }
        }

        if(!found) {
          return false;
        }
      }
      return expectedA.empty();
    }

    struct PhysicsPairs {
      PhysicsPairs(TestGame& game) {
        std::tie(std::ignore, collisionA, collisionB) = game.builder().query<NarrowphaseTableTag, CollisionPairIndexA, CollisionPairIndexB>().get(0);
        std::tie(std::ignore, constraintA, constraintB, constraintEnabled) = game.builder().query<ConstraintsCommonTableTag, CollisionPairIndexA, CollisionPairIndexB, ConstraintData::IsEnabled>().get(0);
      }

      CollisionPairIndexA* collisionA{};
      CollisionPairIndexB* collisionB{};
      CollisionPairIndexA* constraintA{};
      CollisionPairIndexB* constraintB{};
      ConstraintData::IsEnabled* constraintEnabled{};
    };

    static bool _unorderedCollisionPairsMatch(TestGame& game, std::vector<StableElementID> expectedA, std::vector<StableElementID> expectedB) {
      PhysicsPairs pairs{ game };
      return _unorderedIdsMatch(*pairs.collisionA, *pairs.collisionB, expectedA, expectedB, nullptr) &&
        _unorderedIdsMatch(*pairs.constraintA, *pairs.constraintB, expectedA, expectedB, pairs.constraintEnabled);
    }

    static void _updatePhysics(TestGame& game) {
      std::unique_ptr<IAppBuilder> builder = GameBuilder::create(*game.db);
      TableAdapters::getGameConfigMutable(game.builder())->physics.mForcedTargetWidth = 1;
      PhysicsSimulation::updatePhysics(*builder);
      game.execute(std::move(builder));
    }

    static void _assertEnabledContactConstraintCount(TestGame& game, size_t expected) {
      ConstraintsTableMappings* mappings = game.builder().query<SharedRow<ConstraintsTableMappings>>().tryGetSingletonElement();
      auto& isEnabled = game.builder().query<ConstraintData::IsEnabled>().get<0>(0);
      auto& sharedMass = game.builder().query<SharedMassConstraintsTableTag>().get<0>(0);
      const size_t contactCount = mappings->mZeroMassStartIndex;
      size_t enabled = 0;
      for(size_t i = 0; i < contactCount; ++i) {
        if(CollisionMask::shouldSolveConstraint(isEnabled.at(i))) {
          ++enabled;
        }
      }
      Assert::AreEqual(expected, enabled);
      Assert::AreEqual(contactCount, sharedMass.size());
    }

    static void _assertEnabledStaticContactConstraintCount(TestGame& game, size_t expected) {
      auto mappings = game.builder().query<SharedRow<ConstraintsTableMappings>>().tryGetSingletonElement();
      auto isEnabled = game.builder().query<ConstraintData::IsEnabled>().get<0>(0);
      auto sharedMass = game.builder().query<SharedMassConstraintsTableTag>().get<0>(0);
      auto zeroMass = game.builder().query<ZeroMassConstraintsTableTag>().get<0>(0);

      const size_t staticCount = isEnabled.size() - mappings->mZeroMassStartIndex;
      size_t enabled = 0;
      for(size_t i = 0; i < staticCount; ++i) {
        if(CollisionMask::shouldSolveConstraint(isEnabled.at(i + mappings->mZeroMassStartIndex))) {
          ++enabled;
        }
      }
      Assert::AreEqual(expected, enabled);
      Assert::AreEqual(staticCount, zeroMass.size());
    }

    static size_t _getConstraintIndexFromContact(TestGame& game, size_t contact) {
      StableElementID id = game.builder().query<ConstraintElement>().get<0>(0).at(contact);
      auto unpacked = game.builder().getIDResolver()->tryResolveAndUnpack(id);
      Assert::IsTrue(unpacked.has_value());
      return unpacked->unpacked.getElementIndex();
    }

    template<class T>
    static bool isUnorderedEqual(const T& expectedA, const T& expectedB, const T& actualA, const T& actualB) {
      if(expectedA == actualA) {
        return expectedB == actualB;
      }
      return expectedA == actualB && expectedB == actualA;
    }

    static StableElementID getStableID(TestGame& game, size_t index, const UnpackedDatabaseElementID& table) {
      return StableOperations::getStableID(game.builder().query<StableIDRow>(table).get<0>(0), table.remakeElement(index));
    }

    TEST_METHOD(GameOneObject_Migrate_PhysicsDataPreserved) {
      TestGame game;
      GameArgs gameArgs;
      gameArgs.fragmentCount = 1;
      gameArgs.playerPos = glm::vec2(5, 5);
      game.init(gameArgs);
      StableElementID playerId = getStableID(game, 0, game.tables.player);
      StableElementID objectId = getStableID(game, 0, game.tables.fragments);
      const size_t originalObjectStableId = objectId.mStableID;
      const float minCorrection = 0.1f;
      TransformAdapter playerTransform = TableAdapters::getTransform(game.builder(), game.tables.player);
      TransformAdapter fragmentTransform = TableAdapters::getTransform(game.builder(), game.tables.fragments);
      TransformAdapter completedFragmentTransform = TableAdapters::getTransform(game.builder(), game.tables.completedFragments);
      PhysicsObjectAdapter playerPhysics = TableAdapters::getPhysics(game.builder(), game.tables.player);
      PhysicsObjectAdapter fragmentPhysics = TableAdapters::getPhysics(game.builder(), game.tables.fragments);
      PhysicsPairs pairs{ game };

      auto setInitialPos = [&] {
        TableAdapters::write(0, { 1.5f, 0.0f }, *playerTransform.posX, *playerTransform.posY);
        TableAdapters::write(0, { 1.0f, 0.0f }, *fragmentTransform.posX, *fragmentTransform.posY);
        playerPhysics.linVelX->at(0) = -0.5f;
        fragmentPhysics.linVelX->at(0) = 0.5f;
      };
      setInitialPos();
      game.update();

      auto assertInitialResolution = [&] {
        Assert::AreEqual(size_t(1), pairs.collisionA->size());
        _assertEnabledContactConstraintCount(game, 1);
        _assertEnabledStaticContactConstraintCount(game, 0);
        Assert::IsTrue(isUnorderedEqual(playerId, objectId, pairs.collisionA->at(0), pairs.collisionB->at(0)));
        const size_t c = _getConstraintIndexFromContact(game, 0);
        Assert::IsTrue(isUnorderedEqual(playerId, objectId, pairs.constraintA->at(c), pairs.constraintB->at(c)));

        Assert::IsTrue(playerPhysics.linVelX->at(0) > -0.5f + minCorrection, L"Player should be pushed away from object");
        Assert::IsTrue(fragmentPhysics.linVelX->at(0) < 0.5f - minCorrection, L"Object should be pushed away from player");
      };
      assertInitialResolution();

      playerTransform.posX->at(0) = 100.0f;
      game.update();

      auto assertNoPairs = [&] {
        Assert::AreEqual(size_t(0), pairs.collisionA->size());
        _assertEnabledContactConstraintCount(game, 0);
        _assertEnabledStaticContactConstraintCount(game, 0);
      };
      assertNoPairs();

      setInitialPos();
      game.update();
      assertInitialResolution();

      game.builder().query<FragmentGoalFoundRow>(game.tables.fragments).get<0>(0).at(0) = true;

      game.update();

      //Migrate will also snap the fragment to its goal, so recenter the player in collision with the new location
      auto setNewPos = [&] {
        const glm::vec2 dest = TableAdapters::read(0, *completedFragmentTransform.posX, *completedFragmentTransform.posY);
        TableAdapters::write(0, dest, *playerTransform.posX, *playerTransform.posY);
      };
      setNewPos();

      auto ids = game.builder().getIDResolver();
      //Object should have moved to the static table, and mapping updated to new unstable id pointing at static table but same stable id
      objectId = *ids->tryResolveStableID(objectId);
      Assert::IsTrue(objectId == StableElementID{ game.tables.completedFragments.remakeElement(0).mValue, originalObjectStableId });

      game.update();
      auto assertStaticCollision = [&] {
        //Similar to before, except now the single constraint is in the static table instead of the dynamic one
        //Pair order is the same, both because player has lower stable id (0) than object (1) but also because object is now static,
        //and static objects always order B in pairs
        Assert::AreEqual(size_t(1), pairs.collisionA->size());
        _assertEnabledContactConstraintCount(game, 0);
        _assertEnabledStaticContactConstraintCount(game, 1);
        const size_t c = _getConstraintIndexFromContact(game, 0);
        Assert::IsTrue(pairs.collisionA->at(0) == playerId);
        Assert::IsTrue(pairs.constraintA->at(c) == playerId);
        Assert::IsTrue(pairs.collisionB->at(0) == objectId);
        Assert::IsTrue(pairs.constraintB->at(c) == objectId);

        Assert::IsTrue(playerPhysics.linVelX->at(0) > -0.5f, L"Player should be pushed away from object");
      };
      assertStaticCollision();

      playerTransform.posX->at(0) = 100.0f;
      game.update();
      assertNoPairs();

      setNewPos();
      game.update();
      assertStaticCollision();
    }

    size_t cellSize(const Broadphase::SweepGrid::Grid& grid, size_t cell) {
      //Elements don't use the free list but have two entries per object
      //Another approach would be user keys minus free list size
      return grid.cells[cell].axis[0].elements.size() / 2;
    }

    size_t trackedCellPairs(const Broadphase::SweepGrid::Grid& grid, size_t cell) {
      return grid.cells[cell].trackedPairs.size();
    }

    TEST_METHOD(BroadphaseBoundaries) {
      Config::PhysicsConfig cfg;
      cfg.broadphase.bottomLeftX = 0.0f;
      cfg.broadphase.bottomLeftY = 0.0f;
      cfg.broadphase.cellCountX = 2;
      cfg.broadphase.cellCountY = 1;
      cfg.broadphase.cellSizeX = 10.0f;
      cfg.broadphase.cellSizeY = 10.0f;
      //Use negative padding to make the cell size 0.5 instead of 0.7 so overlapping cells are also colliding
      cfg.broadphase.cellPadding = 0.5f - SweepNPruneBroadphase::BoundariesConfig::UNIT_CUBE_EXTENTS;
      TestGame game{ GameConstructArgs{ {}, cfg } };
      GameArgs args;
      args.fragmentCount = 3;
      game.init(args);
      auto task = game.builder();
      const Broadphase::SweepGrid::Grid& grid = *task.query<SharedRow<Broadphase::SweepGrid::Grid>>().tryGetSingletonElement();

      TransformAdapter objs = TableAdapters::getTransform(task, game.tables.fragments);
      std::array bounds = {
        glm::vec2{ 50.0f, 0.0f },
        glm::vec2{ -50.0f, 0.0f },
        glm::vec2{ 0.0f, 50.0f },
        glm::vec2{ 0.0f, -50.0f },
        glm::vec2{ 50.0f, 50.0f },
        glm::vec2{ -50.0f, -50.0f }
      };
      for(const glm::vec2& b : bounds) {
        //Try putting them outside the grid to make sure it clamps properly
        for(size_t i = 0; i < args.fragmentCount; ++i) {
          objs.posX->at(i) = b.x;
          objs.posY->at(i) = b.y;
        }

        game.update();

        _assertEnabledContactConstraintCount(game, 3);
      }

      //One in left cell, one in right, and one on the boundary, all touching
      for(size_t i = 0; i < args.fragmentCount; ++i) {
        objs.posY->at(i) = 0.0f;
      }
      const float halfSize = 0.5f;
      const float padding = 0.1f;
      objs.posX->at(0) = cfg.broadphase.cellSizeX - halfSize - padding;
      objs.posX->at(1) = cfg.broadphase.cellSizeX;
      objs.posX->at(2) = cfg.broadphase.cellSizeX + halfSize + padding;

      game.update();

      Assert::AreEqual(size_t(2), cellSize(grid, 0));
      Assert::AreEqual(size_t(2), cellSize(grid, 1));
      Assert::AreEqual(size_t(1), trackedCellPairs(grid, 0));
      Assert::AreEqual(size_t(1), trackedCellPairs(grid, 1));
      _assertEnabledContactConstraintCount(game, 2);

      //Move boundary object to the right cell
      objs.posX->at(1) = cfg.broadphase.cellSizeX + halfSize*2;
      objs.posX->at(0) = cfg.broadphase.cellSizeX - halfSize - padding;
      objs.posX->at(2) = cfg.broadphase.cellSizeX + halfSize + padding;

      game.update();

      Assert::AreEqual(size_t(1), cellSize(grid, 0));
      Assert::AreEqual(size_t(2), cellSize(grid, 1));
      Assert::AreEqual(size_t(0), trackedCellPairs(grid, 0));
      Assert::AreEqual(size_t(1), trackedCellPairs(grid, 1));
      _assertEnabledContactConstraintCount(game, 1);

      //Move boundary object to the left cell
      objs.posX->at(1) = cfg.broadphase.cellSizeX - halfSize*2;
      objs.posX->at(0) = cfg.broadphase.cellSizeX - halfSize - padding;
      objs.posX->at(2) = cfg.broadphase.cellSizeX + halfSize + padding;

      game.update();

      Assert::AreEqual(size_t(2), cellSize(grid, 0));
      Assert::AreEqual(size_t(1), cellSize(grid, 1));
      Assert::AreEqual(size_t(1), trackedCellPairs(grid, 0));
      Assert::AreEqual(size_t(0), trackedCellPairs(grid, 1));
      _assertEnabledContactConstraintCount(game, 1);

      //Two objects on a boundary
      auto setBoundary = [&] {
        objs.posX->at(0) = cfg.broadphase.cellSizeX;
        objs.posX->at(1) = cfg.broadphase.cellSizeX;
        objs.posX->at(2) = 100.0f;
        game.update();
      };

      setBoundary();

      Assert::AreEqual(size_t(2), cellSize(grid, 0));
      Assert::AreEqual(size_t(3), cellSize(grid, 1));
      Assert::AreEqual(size_t(1), trackedCellPairs(grid, 0));
      Assert::AreEqual(size_t(1), trackedCellPairs(grid, 1));
      _assertEnabledContactConstraintCount(game, 1);

      setBoundary();
      objs.posX->at(0) = cfg.broadphase.cellSizeX + halfSize + padding;

      game.update();

      Assert::AreEqual(size_t(1), cellSize(grid, 0));
      Assert::AreEqual(size_t(3), cellSize(grid, 1));
      Assert::AreEqual(size_t(0), trackedCellPairs(grid, 0));
      Assert::AreEqual(size_t(1), trackedCellPairs(grid, 1));
      _assertEnabledContactConstraintCount(game, 1);
    }

    TEST_METHOD(GameTwoObjects_Migrate_PhysicsDataPreserved) {
      GameArgs gameArgs;
      gameArgs.fragmentCount = 2;
      gameArgs.playerPos = glm::vec2{ 5, 5 };
      TestGame game;
      game.init(gameArgs);
      auto task = game.builder();
      auto ids = task.getIDResolver();
      TransformAdapter fragmentTransform = TableAdapters::getTransform(task, game.tables.fragments);
      PhysicsObjectAdapter fragmentPhysics = TableAdapters::getPhysics(task, game.tables.fragments);
      TransformAdapter playerTransform = TableAdapters::getTransform(task, game.tables.player);
      PhysicsObjectAdapter playerPhysics = TableAdapters::getPhysics(task, game.tables.player);

      StableIDRow& stablePlayer = task.query<StableIDRow>(game.tables.player).get<0>(0);
      StableIDRow& stableFragment = task.query<StableIDRow>(game.tables.fragments).get<0>(0);
      StableElementID playerId = StableOperations::getStableID(stablePlayer, game.tables.player.remakeElement(0));
      StableElementID objectLeftId = StableOperations::getStableID(stableFragment, game.tables.fragments.remakeElement(0));
      StableElementID objectRightId = StableOperations::getStableID(stableFragment, game.tables.fragments.remakeElement(1));

      float initialX[] = { 1.0f, 2.0f, 1.5f };
      float initialY[] = { 1.0f, 1.0f, 1.75f };
      for(size_t i = 0; i < 2; ++i) {
        fragmentTransform.posX->at(i) = initialX[i];
        fragmentTransform.posY->at(i) = initialY[i];
        fragmentPhysics.linVelY->at(i) = 0.5f;
      }
      glm::vec2 initialRight{ TableAdapters::read(1, *fragmentTransform.posX, *fragmentTransform.posY) };

      auto setInitialPlayerPos = [&] {
        playerTransform.posX->at(0) = initialX[2];
        playerTransform.posY->at(0) = initialY[2];
        playerPhysics.linVelY->at(0) = -0.5f;
      };
      setInitialPlayerPos();

      game.update();

      PhysicsPairs pairs{ game };
      Assert::AreEqual(size_t(3), pairs.collisionA->size());
      _assertEnabledContactConstraintCount(game, 3);
      _assertEnabledStaticContactConstraintCount(game, 0);
      Assert::IsTrue(_unorderedCollisionPairsMatch(game,
        { playerId, playerId, objectLeftId },
        { objectLeftId, objectRightId, objectRightId }
      ));

      //Min ia a bit weirder here since the impulse is spread between the two objects and at an angle
      const float minCorrection = 0.05f;
      Assert::IsTrue(playerPhysics.linVelY->at(0) > -0.5f + minCorrection, L"Player should be pushed away from object");
      Assert::IsTrue(fragmentPhysics.linVelY->at(0) < 0.5f - minCorrection, L"Object should be pushed away from player");
      Assert::IsTrue(fragmentPhysics.linVelY->at(1) < 0.5f - minCorrection, L"Object should be pushed away from player");

      auto&& [goalFound, goalX, goalY] = task.query<FragmentGoalFoundRow, FloatRow<Tags::FragmentGoal, Tags::X>, FloatRow<Tags::FragmentGoal, Tags::Y>>().get(0);
      goalFound->at(0) = true;
      TableAdapters::write(0, glm::vec2{ initialX[0], initialY[0] }, *goalX, *goalY);
      game.update();

      //Need to update both since one moved and the other was affected by swap removal
      objectLeftId = *ids->tryResolveStableID(objectLeftId);
      objectRightId = *ids->tryResolveStableID(objectRightId);

      auto resetStaticPos = [&] {
        setInitialPlayerPos();
        fragmentTransform.posX->at(0) = initialRight.x;
        fragmentTransform.posY->at(0) = initialRight.y + 0.1f;
        fragmentPhysics.linVelY->at(0) = 0.5f;
      };

      resetStaticPos();
      game.update();

      auto assertStaticCollision = [&] {
        Assert::AreEqual(size_t(3), pairs.collisionA->size());
        _assertEnabledContactConstraintCount(game, 1);
        _assertEnabledStaticContactConstraintCount(game, 2);
        //Now left is the B object always since it's static
        Assert::IsTrue(_unorderedCollisionPairsMatch(game,
          { playerId, playerId, objectRightId },
          { objectLeftId, objectRightId, objectLeftId }
        ));

        Assert::IsTrue(playerPhysics.linVelY->at(0) > -0.5f + minCorrection, L"Player should be pushed away from object");
        Assert::IsTrue(fragmentPhysics.linVelY->at(0) < 0.5f - minCorrection, L"Object should be pushed away from player");
      };
      assertStaticCollision();

      resetStaticPos();
      playerTransform.posX->at(0) = 100.0f;
      game.update();

      Assert::AreEqual(size_t(1), pairs.collisionA->size());
      _assertEnabledContactConstraintCount(game, 0);
      _assertEnabledStaticContactConstraintCount(game, 1);
      Assert::IsTrue(_unorderedCollisionPairsMatch(game,
        { objectRightId },
        { objectLeftId }
      ));

      resetStaticPos();
      game.update();

      assertStaticCollision();
    }

    static StableElementID _addCollisionPair(TestGame& game, StableElementID a, StableElementID b) {
      auto task = game.builder();
      auto query = task.query<CollisionPairIndexA, CollisionPairIndexB, NarrowphaseTableTag, StableIDRow>();
      auto&& [pairA, pairB, _, stable] = query.get(0);
      const UnpackedDatabaseElementID collisionTable = query.matchingTableIDs[0];
      auto modifier = task.getModifierForTable(collisionTable);
      const size_t i = modifier->addElements(1);
      pairA->at(i) = a;
      pairB->at(i) = b;

      auto changedPairs = task.query<SharedRow<SweepNPruneBroadphase::ChangedCollisionPairs>>().tryGetSingletonElement();
      StableElementID result = StableOperations::getStableID(*stable, collisionTable.remakeElement(i));
      changedPairs->mGained.push_back(result);
      return result;
    }

    static StableElementID _getGameobjectID(TestGame& game, size_t index) {
      return StableOperations::getStableID(game.builder().query<StableIDRow>(game.tables.fragments).get<0>(0), game.tables.fragments.remakeElement(index));
    }

    //Hack to match ispc enum
    static constexpr int NO_SYNC = 0;
    static constexpr int SYNC_TO_A = 1;
    static constexpr int SYNC_TO_B = 2;

    TEST_METHOD(ConstraintsTableBuilder_Integration) {
      GameArgs args;
      args.fragmentCount = 5;
      args.forcedPadding = 1;
      TestGame game;
      game.init(args);

      StableElementID a = _getGameobjectID(game, 0);
      StableElementID b = _getGameobjectID(game, 1);
      StableElementID pair = _addCollisionPair(game, a, b);

      _buildConstraintsTable(game);

      auto&& [common, syncTypeA, syncTypeB] = game.builder().query<ConstraintsCommonTableTag,
        ConstraintObject<ConstraintObjA>::SyncType,
        ConstraintObject<ConstraintObjB>::SyncType>().get(0);
      Assert::AreEqual(size_t(2), common->size(), L"Should have one real element and one padding");
      Assert::AreEqual(NO_SYNC, syncTypeA->at(0));
      Assert::AreEqual(NO_SYNC, syncTypeB->at(0));
    }

    static ConstraintsTableMappings* getConstraintMappings(TestGame& game) {
      return game.builder().query<SharedRow<ConstraintsTableMappings>>().tryGetSingletonElement();
    }

    static auto getTargetElementRange(TestGame& game, size_t tableId) {
      const size_t commonSize = game.builder().query<ConstraintsCommonTableTag>().get<0>(0).size();
      const PhysicsTableIds ids = Physics::getTableIds(*game.testBuilder);
      return ctbdetails::getTargetElementRange(tableId, ids, *getConstraintMappings(game), commonSize);
    }

    static StableElementID tryTakeSuitableFreeSlot(TestGame& game, size_t tableId, const StableElementID& a, const StableElementID& b, size_t width) {
      PhysicsPairs pairs{ game };
      auto ids = game.builder().getIDResolver();
      StableElementID result = ctbdetails::tryTakeSuitableFreeSlot(0,
        tableId,
        getTargetElementRange(game, tableId),
        *getConstraintMappings(game),
        Physics::getTableIds(*game.testBuilder),
        a,
        b,
        *pairs.constraintA,
        *pairs.constraintB,
        *ids,
        width);

      if(result != StableElementID::invalid()) {
        StableElementID p = _addCollisionPair(game, a, b);
        ConstraintsTableBuilder::AddDeps deps{ ConstraintsTableBuilder::AddDeps::query(game.builder()) };
        ConstraintsTableBuilder::assignConstraint(p, a, b, result, deps);
      }
      return result;
    }

    void addPaddingToTable(TestGame& game, size_t table, size_t amount) {
      ConstraintsTableBuilder::AddDeps deps{ ConstraintsTableBuilder::AddDeps::query(game.builder()) };
      ConstraintsTableBuilder::addPaddingToTable(table, amount, Physics::getTableIds(*game.testBuilder), deps);
    }

    TEST_METHOD(CTBDetails) {
      TestGame game;
      GameArgs args;
      args.fragmentCount = 5;
      args.forcedPadding = 2;
      game.init(args);

      const PhysicsTableIds tableIds = Physics::getTableIds(*game.testBuilder);

      StableElementID a = _getGameobjectID(game, 0);
      StableElementID b  = _getGameobjectID(game, 1);
      StableElementID c = _getGameobjectID(game, 2);
      StableElementID d = _getGameobjectID(game, 3);

      auto range = getTargetElementRange(game, tableIds.mSharedMassConstraintTable);
      Assert::IsTrue(range == std::make_pair(size_t(0), size_t(0)));
      range = getTargetElementRange(game, tableIds.mZeroMassConstraintTable);
      Assert::IsTrue(range == std::make_pair(size_t(0), size_t(0)));

      //Should fail because nothing is there
      Assert::IsTrue(StableElementID::invalid() == tryTakeSuitableFreeSlot(game, tableIds.mSharedMassConstraintTable, a, b, *args.forcedPadding));
      Assert::IsTrue(StableElementID::invalid() == tryTakeSuitableFreeSlot(game, tableIds.mZeroMassConstraintTable, a, b, *args.forcedPadding));

      addPaddingToTable(game, tableIds.mZeroMassConstraintTable, 1);

      //Should fail because the space was added to the zero mass table
      Assert::IsTrue(StableElementID::invalid() == tryTakeSuitableFreeSlot(game, tableIds.mSharedMassConstraintTable, a, b, *args.forcedPadding));
      StableElementID ab = tryTakeSuitableFreeSlot(game, tableIds.mZeroMassConstraintTable, a, b, *args.forcedPadding);
      Assert::AreEqual(size_t(0), ab.mUnstableIndex & tableIds.mElementIDMask);

      addPaddingToTable(game, tableIds.mZeroMassConstraintTable, 1);

      Assert::IsTrue(StableElementID::invalid() == tryTakeSuitableFreeSlot(game, tableIds.mZeroMassConstraintTable, a, c, *args.forcedPadding), L"Should not accept slots within target width of non-static object");
      StableElementID cb = tryTakeSuitableFreeSlot(game, tableIds.mZeroMassConstraintTable, c, b, *args.forcedPadding);
      Assert::AreEqual(size_t(1), cb.mUnstableIndex & tableIds.mElementIDMask, L"Static elements should ignore padding rules");

      addPaddingToTable(game, tableIds.mSharedMassConstraintTable, 1);

      StableElementID ad = tryTakeSuitableFreeSlot(game, tableIds.mSharedMassConstraintTable, a, d, *args.forcedPadding);
      Assert::AreEqual(size_t(0), ad.mUnstableIndex & tableIds.mElementIDMask, L"Elements in dynamic section should ignore padding rules in neighboring sections");

      addPaddingToTable(game, tableIds.mSharedMassConstraintTable, 1);

      Assert::IsTrue(StableElementID::invalid() == tryTakeSuitableFreeSlot(game, tableIds.mSharedMassConstraintTable, a, b, *args.forcedPadding), L"Object A should obey padding rules");
      Assert::IsTrue(StableElementID::invalid() == tryTakeSuitableFreeSlot(game, tableIds.mSharedMassConstraintTable, b, d, *args.forcedPadding), L"Object B should obey padding rules");
      StableElementID bc = tryTakeSuitableFreeSlot(game, tableIds.mSharedMassConstraintTable, b, c, *args.forcedPadding);
      Assert::AreEqual(size_t(1), bc.mUnstableIndex & tableIds.mElementIDMask, L"Object passing padding rules should find free slot");
    }

    TEST_METHOD(StableInsert) {
      TestStableDB db;
      StableTableA& table = std::get<StableTableA>(db.mTables);
      Row<int>& values = std::get<Row<int>>(table.mRows);
      StableIDRow& ids = std::get<StableIDRow>(table.mRows);
      StableElementMappings mappings;

      TableOperations::stableInsertRangeAt(table, UnpackedDatabaseElementID::fromPacked(TestStableDB::getElementID<StableTableA>(1)), 5, mappings);

      Assert::AreEqual(size_t(5), TableOperations::size(table));
      for(int i = 0; i < 5; ++i) {
        values.at(i) = i;
      }

      TableOperations::stableInsertRangeAt(table, UnpackedDatabaseElementID::fromPacked(TestStableDB::getElementID<StableTableA>(5)), 1, mappings);
      Assert::AreEqual(size_t(6), TableOperations::size(table));
      Assert::AreEqual(4, values.at(4));
      values.at(5) = 5;
      StableElementID stable = StableOperations::getStableID(ids, UnpackedDatabaseElementID::fromPacked(TestStableDB::getElementID<StableTableA>(3)));

      TableOperations::stableInsertRangeAt(table, UnpackedDatabaseElementID::fromPacked(TestStableDB::getElementID<StableTableA>(0)), 5, mappings);
      Assert::AreEqual(size_t(11), TableOperations::size(table));
      for(size_t i = 0; i < 5; ++i) {
        Assert::AreEqual(int(i), values.at(i + 5), L"Values should have been shifted over by previous insertion");
      }
      std::optional<StableElementID> resolved = StableOperations::tryResolveStableIDWithinTable<TestStableDB>(stable, ids, mappings);
      Assert::IsTrue(resolved.has_value());
      constexpr size_t elementMask = TestStableDB::ElementID::ELEMENT_INDEX_MASK;
      Assert::AreEqual(3, values.at(resolved->mUnstableIndex & elementMask));
    }

    TEST_METHOD(UnpackedDBElementID) {
      auto id = GameDatabase::getElementID<GameObjectTable>(5);
      UnpackedDatabaseElementID unpacked = UnpackedDatabaseElementID::fromPacked(id);
      Assert::AreEqual(id.getElementIndex(), unpacked.getElementIndex());
      Assert::AreEqual(id.getTableIndex(), unpacked.getTableIndex());
      Assert::AreEqual(id.ELEMENT_INDEX_MASK, unpacked.getElementMask());

      unpacked = UnpackedDatabaseElementID::fromElementMask(id.ELEMENT_INDEX_MASK, id.getShiftedTableIndex(), id.getElementIndex());
      Assert::AreEqual(id.getElementIndex(), unpacked.getElementIndex());
      Assert::AreEqual(id.getTableIndex(), unpacked.getTableIndex());
      Assert::AreEqual(id.ELEMENT_INDEX_MASK, unpacked.getElementMask());
    }

    struct TestStatInfo {
      int lambdaInvocations{};
      bool shouldRun{};
    };

    static void addAllStats(IAppBuilder& builder, TestStatInfo& test) {
      auto task = builder.createTask();
      task.setName("test");
      auto once = std::make_shared<bool>(true);
      KnownTables tables{ builder };
      auto query = task.query<const StableIDRow>(tables.fragments);
      auto ids = task.getIDResolver();
      task.setCallback([once, query, &test, ids](AppTaskArgs& args) mutable {
        auto [stableRow] = query.get(0);
        if(!test.shouldRun) {
          return;
        }

        const StableElementID idA = StableOperations::getStableID(*stableRow, query.matchingTableIDs[0].remakeElement(0));
        const StableElementID idB = StableOperations::getStableID(*stableRow, query.matchingTableIDs[0].remakeElement(1));
        const StableElementID idC = StableOperations::getStableID(*stableRow, query.matchingTableIDs[0].remakeElement(2));

        {
          LambdaStatEffectAdapter lambda = TableAdapters::getLambdaEffects(args);
          const size_t id = TableAdapters::addStatEffectsSharedLifetime(lambda.base, StatEffect::INSTANT, &idA.mStableID, 1);
          lambda.command->at(id) = [&test, ids](LambdaStatEffect::Args& args) {
            Assert::AreEqual(size_t(0), ids->uncheckedUnpack(args.resolvedID).getElementIndex());
            ++test.lambdaInvocations;
          };
        }
        {
          VelocityStatEffectAdapter vel = TableAdapters::getVelocityEffects(args);
          const size_t id = TableAdapters::addStatEffectsSharedLifetime(vel.base, 2, &idB.mStableID, 1);
          VelocityStatEffect::VelocityCommand& vcmd = vel.command->at(id);
          vcmd.linearImpulse = glm::vec2(1.0f);
          vcmd.angularImpulse = 1.0f;
        }
        {
          PositionStatEffectAdapter pos = TableAdapters::getPositionEffects(args);
          const size_t id = TableAdapters::addStatEffectsSharedLifetime(pos.base, 1, &idC.mStableID, 1);
          PositionStatEffect::PositionCommand& pcmd = pos.command->at(id);
          pcmd.pos = glm::vec2(5.0f);
          pcmd.rot = glm::vec2(0.0f, 1.0f);
        }
      });

      builder.submitTask(std::move(task));
    }

    TEST_METHOD(Stats) {
      GameArgs args;
      constexpr size_t OBJ_COUNT = 3;
      args.fragmentCount = OBJ_COUNT;
      GameConstructArgs construct;
      TestStatInfo test;
      construct.updateConfig.injectGameplayTasks = [&](IAppBuilder& args) { addAllStats(args, test); };
      TestGame game{ std::move(construct) };
      game.init(args);

      PhysicsObjectAdapter fragmentPhysics = TableAdapters::getPhysics(game.builder(), game.tables.fragments);
      TransformAdapter fragmentTransform = TableAdapters::getTransform(game.builder(), game.tables.fragments);
      for(size_t i = 0; i < OBJ_COUNT; ++i) {
        //Need to move them away from the fragment completion location
        fragmentTransform.posX->at(i) = 4.0f;
      }

      auto lambdaQuery = game.builder().query<LambdaStatEffect::LambdaRow, StatEffect::Lifetime>();
      auto velQuery = game.builder().query<VelocityStatEffect::CommandRow, StatEffect::Lifetime>();
      auto posQuery = game.builder().query<PositionStatEffect::CommandRow, StatEffect::Lifetime>();
      auto [lambdaCmd, lambdaLife] = lambdaQuery.get(0);
      auto [velCmd, velLife] = velQuery.get(0);
      auto [posCmd, posLife] = posQuery.get(0);

      test.shouldRun = true;
      game.update();
      test.shouldRun = false;

      Assert::AreEqual(1, test.lambdaInvocations);

      Assert::AreEqual(1.0f, fragmentPhysics.linVelX->at(1), 0.1f);
      Assert::AreEqual(1.0f, fragmentPhysics.linVelY->at(1), 0.1f);
      Assert::AreEqual(1.0f, fragmentPhysics.angVel->at(1), 0.1f);
      Assert::AreEqual(size_t(1), velLife->size());
      Assert::AreEqual(size_t(1), velLife->at(0));

      Assert::AreEqual(5.0f, fragmentTransform.posX->at(2));
      Assert::AreEqual(5.0f, fragmentTransform.posY->at(2));
      Assert::AreEqual(0.0f, fragmentTransform.rotX->at(2));
      Assert::AreEqual(1.0f, fragmentTransform.rotY->at(2));
      Assert::AreEqual(size_t(1), posLife->size());
      Assert::AreEqual(size_t(0), posLife->at(0));

      //Set the values to something to show they don't change after the next update
      fragmentPhysics.linVelX->at(1) = 0.0f;
      fragmentTransform.posX->at(2) = 10.0f;

      //After this pos and lambda should be removed and should not have executed again before removal
      game.update();

      Assert::AreEqual(1, test.lambdaInvocations);
      Assert::AreEqual(size_t(0), lambdaLife->size());
      Assert::AreEqual(size_t(0), posLife->size());
      //Assert the unchanged values
      Assert::AreEqual(1.0f, fragmentPhysics.linVelX->at(1), 0.1f);
      Assert::AreEqual(10.0f, fragmentTransform.posX->at(2), 0.1f);

      fragmentPhysics.linVelX->at(1) = 0.0f;

      //Should remove velcity
      game.update();

      Assert::AreEqual(size_t(0), velLife->size());
      Assert::AreEqual(0.0f, fragmentPhysics.linVelX->at(1), 0.1f);
    }

    TEST_METHOD(PlayerInputTest) {
      GameArgs args;
      args.playerPos = glm::vec2{ 0.0f };
      TestGame game{ args };
      auto [input, posX] = game.builder().query<Row<PlayerInput>, FloatRow<Tags::Pos, Tags::X>>().get(0);
      input->at(0).mMoveX = 1.0f;

      //Once to compute the impulse, next frame updates position with it
      game.update();
      game.update();

      Assert::IsTrue(posX->at(0) > 0.0f);
    }
/*
    TEST_METHOD(GameplayExtract) {
      GameArgs args;
      args.fragmentCount = 1;
      args.completedFragmentCount = 1;
      args.playerPos = glm::vec2{ 0.0f };
      TestGame game{ args };
      GameObjectAdapter fragment = TableAdapters::getGameObjects(game);
      GameObjectAdapter completedFragment = TableAdapters::getStaticGameObjects(game);
      PlayerAdapter player = TableAdapters::getPlayer(game);
      auto setValues = [](GameObjectAdapter obj, float offset) {
        obj.transform.posX->at(0) = 1.0f + offset;
        obj.transform.posY->at(0) = 2.0f + offset;
        obj.transform.rotX->at(0) = std::cos(3.0f + offset);
        obj.transform.rotY->at(0) = std::sin(3.0f + offset);
        if(obj.physics.linVelX) {
          obj.physics.linVelX->at(0) = 0.1f + offset;
          obj.physics.linVelY->at(0) = 0.2f + offset;
          obj.physics.angVel->at(0) = 0.3f + offset;
        }
      };

      setValues(TableAdapters::getGameObjects(game), 1.0f);
      setValues(TableAdapters::getStaticGameObjects(game), 2.0f);
      setValues(TableAdapters::getPlayer(game).object, 3.0f);

      game.update();

      auto assertValues = [](GameObjectAdapter obj, float offset) {
        Assert::AreEqual(1.0f + offset, obj.transform.posX->at(0));
        Assert::AreEqual(2.0f + offset, obj.transform.posY->at(0));
        Assert::AreEqual(std::cos(3.0f + offset), obj.transform.rotX->at(0));
        Assert::AreEqual(std::sin(3.0f + offset), obj.transform.rotY->at(0));
        if(obj.physics.linVelX) {
          Assert::AreEqual(0.1f + offset, obj.physics.linVelX->at(0));
          Assert::AreEqual(0.2f + offset, obj.physics.linVelY->at(0));
          Assert::AreEqual(0.3f + offset, obj.physics.angVel->at(0));
        }
      };

      assertValues(TableAdapters::getGameplayGameObjects(game), 1.0f);
      assertValues(TableAdapters::getGameplayStaticGameObjects(game), 2.0f);
      assertValues(TableAdapters::getGameplayPlayer(game).object, 3.0f);
    }

    TEST_METHOD(GlobalPointForce) {
      GameArgs args;
      args.fragmentCount = 1;
      TestGame game{ args };

      GameObjectAdapter fragment = TableAdapters::getGameObjects(game);
      fragment.transform.posX->at(0) = 5.0f;
      fragment.transform.rotX->at(0) = 1.0f;

      AreaForceStatEffectAdapter effect = TableAdapters::getAreaForceEffects(game, 0);
      TableAdapters::addStatEffectsSharedLifetime(effect.base, StatEffect::INSTANT, nullptr, 1);

      AreaForceStatEffect::Command& cmd = effect.command->at(0);
      cmd.origin = glm::vec2{ 4.0f, 0.0f };
      cmd.direction = glm::vec2{ 1.0f, 0.0f };
      cmd.dynamicPiercing = 3.0f;
      cmd.terrainPiercing = 0.0f;
      cmd.rayCount = 4;
      AreaForceStatEffect::Command::Cone cone;
      cone.halfAngle = 0.25f;
      cone.length = 15.0f;
      cmd.shape = cone;
      AreaForceStatEffect::Command::FlatImpulse impulseType{ 1.0f };
      cmd.impulseType = impulseType;

      //One update to request the impulse, then the next to apply it
      game.update();
      game.update();

      Assert::IsTrue(fragment.physics.linVelX->at(0) > 0.0f);
      Assert::AreEqual(size_t(0), effect.command->size());
    }

    TEST_METHOD(Config) {
      GameConfig gameConfig;
      gameConfig.fragment.fragmentColumns = 5;

      std::string serialized = ConfigIO::serializeJSON(gameConfig);
      ConfigIO::Result r = ConfigIO::deserializeJson(serialized, *Config::createFactory());
      ConfigIO::Result r2 = ConfigIO::deserializeJson("test", *Config::createFactory());

      Assert::AreEqual(size_t(0), r.value.index(), L"Parse should succeed");
      Assert::AreEqual(size_t(1), r2.value.index(), L"Parse should fail");
      Assert::AreEqual(size_t(5), std::get<0>(r.value).fragment.fragmentColumns);
    }

    void assertQueryHas(StableElementID query, std::vector<StableElementID> expected, SpatialQueryAdapter& queries) {
        const SpatialQuery::Result* r = SpatialQuery::tryGetResult(query, queries);
        Assert::IsNotNull(r);

        std::visit([&expected](const auto& r) {
          Assert::AreEqual(expected.size(), r.results.size(), L"Size should match");
          for(size_t i = 0; i < expected.size(); ++i) {
            Assert::AreEqual(expected[i].mStableID, r.results[i].id.mStableID);
          }
        }, r->result);
    }

    void assertQueryHasObject(StableElementID query, StableElementID object, SpatialQueryAdapter& queries) {
      assertQueryHas(query, { object }, queries);
    }

    void assertQueryNoObjects(StableElementID query, SpatialQueryAdapter& queries) {
      assertQueryHas(query, {}, queries);
    }

    void tickQueryUpdate(TestGame& game) {
      //Since the updates are deferred it takes two ticks for the new results to show up
      game.update();
      game.update();
    }

    TEST_METHOD(SpatialQueries) {
      GameArgs args;
      args.fragmentCount = 1;
      TestGame game{ args };

      auto objs = TableAdapters::getGameObjects(game);
      const StableElementID objID = StableElementID::fromStableID(objs.stable->at(0));
      auto queries = TableAdapters::getSpatialQueries(game);
      StableElementID bb = SpatialQuery::createQuery(queries, { SpatialQuery::AABB{ glm::vec2(2.0f), glm::vec2(3.5f) } }, 10);
      StableElementID circle = SpatialQuery::createQuery(queries, { SpatialQuery::Circle{ glm::vec2(0, 5), 1.5f } }, 10);
      StableElementID cast = SpatialQuery::createQuery(queries, { SpatialQuery::Raycast{ glm::vec2(-1.0f), glm::vec2(2.0f, -1.0f) } }, 10);
      //Update to create the queries
      game.update();

      //Move object into aabb
      TableAdapters::write(0, glm::vec2(2.5f), *objs.transform.posX, *objs.transform.posY);
      game.update();

      assertQueryHasObject(bb, objID, queries);
      assertQueryNoObjects(circle, queries);
      assertQueryNoObjects(cast, queries);

      //Move aabb out of object and move circle in
      SpatialQuery::refreshQuery(bb, queries, { SpatialQuery::AABB{ glm::vec2(20), glm::vec2(21) } }, 10);
      SpatialQuery::refreshQuery(circle, queries, { SpatialQuery::Circle{ glm::vec2(2), 1.0f } }, 10);
      tickQueryUpdate(game);

      assertQueryNoObjects(bb, queries);
      assertQueryHasObject(circle, objID, queries);
      assertQueryNoObjects(cast, queries);

      //Move object to raycast and circle with it
      TableAdapters::write(0, glm::vec2(2.f, -1.f), *objs.transform.posX, *objs.transform.posY);
      SpatialQuery::refreshQuery(circle, queries, { SpatialQuery::Circle{ glm::vec2(2, -1), 0.5f } }, 10);
      tickQueryUpdate(game);

      assertQueryNoObjects(bb, queries);
      assertQueryHasObject(circle, objID, queries);
      assertQueryHasObject(cast, objID, queries);

      //Move circle out and turn raycast into an aabb
      SpatialQuery::refreshQuery(circle, queries, { SpatialQuery::Circle{ glm::vec2(20), 1.0f } }, 10);
      SpatialQuery::refreshQuery(cast, queries, { SpatialQuery::AABB{ glm::vec2(2.f, -1.f), glm::vec2(2.5f, 0.f) } }, 10);
      tickQueryUpdate(game);

      assertQueryNoObjects(bb, queries);
      assertQueryNoObjects(circle, queries);
      assertQueryHasObject(cast, objID, queries);

      //Try a query with a single tick lifetime
      StableElementID single = SpatialQuery::createQuery(queries, { SpatialQuery::Circle{ glm::vec2(2, -1), 1.f } }, SpatialQuery::SINGLE_USE);
      game.update();
      assertQueryNoObjects(single, queries);
      auto lambda = TableAdapters::getLambdaEffects(game, 0);
      //Query should be resolved during physics then viewable later by gameplay but destroyed at the end of the frame.
      //Catch it in the middle with a lambda stat effect which should execute before the removal but after it's resolved
      const size_t l = TableAdapters::addStatEffectsSharedLifetime(lambda.base, StatEffect::INSTANT, &objID.mStableID, 1);
      lambda.command->at(l) = [&](...) {
        assertQueryHasObject(single, objID, queries);
      };
      game.update();
      Assert::IsFalse(SpatialQuery::getIndex(single, queries).has_value());
    }

    TEST_METHOD(CollisionMasks) {
      GameArgs args;
      args.fragmentCount = 2;
      TestGame game{ args };
      GameObjectAdapter objs = TableAdapters::getGameObjects(game);

      objs.transform.posX->at(0) = 1;
      objs.transform.posX->at(1) = 1.1f;
      objs.physics.collisionMask->at(0) = 1 << 2;
      objs.physics.collisionMask->at(1) = 1 << 1;

      game.update();

      Assert::AreEqual(0.0f, objs.physics.linVelX->at(0));
      Assert::AreEqual(0.0f, objs.physics.linVelX->at(1));

      objs.physics.collisionMask->at(0) |= 1 << 1;

      game.update();

      Assert::AreNotEqual(0.0f, objs.physics.linVelX->at(0));
      Assert::AreNotEqual(0.0f, objs.physics.linVelX->at(1));
    }

    TEST_METHOD(FragmentStateMachine) {
      GameArgs args;
      args.fragmentCount = 2;
      TestGame game{ args };
      FragmentAdapter frag = TableAdapters::getFragments(game);
      GameObjectAdapter objs = TableAdapters::getGameObjects(game);

      StableElementID self = StableElementID::fromStableRow(0, *objs.stable);
      FragmentStateMachine::FragmentState::Variant desiredState{ FragmentStateMachine::SeekHome{} };
      FragmentStateMachine::setState(game, self, FragmentStateMachine::FragmentState::Variant{ desiredState });

      game.update();

      Assert::AreEqual(desiredState.index(), frag.state->at(0).currentState.index());
      StableElementID target = std::get<FragmentStateMachine::SeekHome>(frag.state->at(0).currentState).target;

      std::get<FragmentGoalFoundRow>(std::get<GameObjectTable>(game.db.mTables).mRows).at(0) = true;
      Fragment::_migrateCompletedFragments(game, 0);
      game.update();
      game.update();

      StableElementMappings& mappings = TableAdapters::getStableMappings(game);
      Assert::IsFalse(mappings.findKey(target.mStableID).has_value(), L"Target created by state should have been destroyed when object was migrated to a table without state");

      //Do the same state transition for the second object
      self = StableElementID::fromStableRow(0, *objs.stable);
      FragmentStateMachine::setState(game, self, FragmentStateMachine::FragmentState::Variant{ desiredState });

      game.update();

      Assert::AreEqual(desiredState.index(), frag.state->at(0).currentState.index());
      target = std::get<FragmentStateMachine::SeekHome>(frag.state->at(0).currentState).target;

      //Enqueue removal of the object. The destruction should still remove the target
      auto tls = TableAdapters::getThreadLocal(game, 0);
      Events::onRemovedElement(self, game);

      game.update();
      game.update();

      Assert::IsFalse(mappings.findKey(target.mStableID).has_value(), L"Target created by state should have been destroyed before object was destroyed");
    }
    */
  };
}