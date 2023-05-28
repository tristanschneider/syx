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

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Test {
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
  };

  struct TestGame {
    TestGame() {
      Scheduler& scheduler = Simulation::_getScheduler(db);
      auto createEmpty = [] { return TaskBuilder::addEndSync(TaskNode::create([](...){})); };
      SimulationPhases phases {
        createEmpty(),
        createEmpty(),
        createEmpty(),
        createEmpty(),
        createEmpty(),
        createEmpty(),
        createEmpty()
      };

      scheduler.mScheduler.Initialize(enki::TaskSchedulerConfig{});

      Simulation::init(db);
      Simulation::buildUpdateTasks(db, phases);
      Simulation::linkUpdateTasks(phases);
      task = TaskBuilder::buildDependencies(phases.root.mBegin);
    }

    TestGame(const GameArgs& args)
      : TestGame() {
      init(args);
    }

    operator GameDB() {
      return { db };
    }

    void init(const GameArgs& args) {
      StableElementMappings& stable = TableAdapters::getStableMappings(*this);

      TableOperations::stableResizeTableDB<GameObjectTable>(db, args.fragmentCount, stable);
      TableOperations::stableResizeTableDB<StaticGameObjectTable>(db, args.completedFragmentCount, stable);

      GlobalsAdapter globals = TableAdapters::getGlobals(*this);
      globals.scene->mState = SceneState::State::Update;
      globals.scene->mBoundaryMin = glm::vec2(-100);
      globals.scene->mBoundaryMax = glm::vec2(100);

      if(args.playerPos) {
        createPlayer(*args.playerPos);
      }

      PhysicsSimulation::initialPopulateBroadphase(*this);
    }

    void createPlayer(const glm::vec2& pos) {
      PlayerAdapter player = TableAdapters::getPlayer(*this);
      auto& stable = TableAdapters::getStableMappings(*this);
      TableOperations::stableResizeTableDB<PlayerTable>(db, 1, stable);

      player.object.transform.posX->at(0) = pos.x;
      player.object.transform.posY->at(0) = pos.y;
      player.object.transform.rotX->at(0) = 1.0f;
      player.object.transform.rotY->at(0) = 0.0f;
    }

    void update() {
      Scheduler& scheduler = Simulation::_getScheduler(db);
      task.mBegin->mTask.addToPipe(scheduler.mScheduler);
      scheduler.mScheduler.WaitforTask(task.mEnd->mTask.get());
    }

    TaskRange task;
    GameDatabase db;
  };


  TEST_CLASS(Tests) {
    //Paste a snapshot and update the link to reproduce a problem case in a test
    TEST_METHOD(ReplaySavedSnapshot) {
      constexpr bool debuggingSnapshot = false;
      if(debuggingSnapshot) {
        const char* path = "C:/syx/dof/bugs/recovery.snap";
        TestGame game;
        Simulation::loadFromSnapshot(game.db, path);
        game.update();
      }
    }

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

    static void _fillNarrowphaseData(GameDatabase& db) {
      auto& pairs = std::get<CollisionPairsTable>(db.mTables);
      _execute(Physics::fillNarrowphaseData<
        FloatRow<Tags::Pos, Tags::X>,
        FloatRow<Tags::Pos, Tags::Y>,
        FloatRow<Tags::Rot, Tags::CosAngle>,
        FloatRow<Tags::Rot, Tags::SinAngle>>(pairs, db, _getStableMappings(db), PhysicsSimulation::_getPhysicsTableIds()));
    }

    static void _fillConstraintVelocities(GameDatabase& db) {
      auto& constraints = std::get<ConstraintCommonTable>(db.mTables);
      _execute(Physics::fillConstraintVelocities<
        FloatRow<Tags::LinVel, Tags::X>,
        FloatRow<Tags::LinVel, Tags::Y>,
        FloatRow<Tags::AngVel, Tags::Angle>>(constraints, db));
    }

    static void _storeConstraintVelocities(GameDatabase& db) {
      auto& constraints = std::get<ConstraintCommonTable>(db.mTables);
      _execute(Physics::storeConstraintVelocities<
        FloatRow<Tags::LinVel, Tags::X>,
        FloatRow<Tags::LinVel, Tags::Y>,
        FloatRow<Tags::AngVel, Tags::Angle>>(constraints, db));
    }

    static StableElementMappings& _getStableMappings(GameDatabase& db) {
      return std::get<SharedRow<StableElementMappings>>(std::get<GlobalGameData>(db.mTables).mRows).at();
    }

    template<class TableT>
    static void _computeBroadphaseBoundaries(size_t index, TableT& table) {
      //Hack to do it on just the one index since this is what's used to determine element count
      SweepNPruneBroadphase::NeedsReinsert needsReinsert;
      needsReinsert.resize(1);
      SweepNPruneBroadphase::recomputeBoundaries(
        TableOperations::_unwrapRowWithOffset<SweepNPruneBroadphase::OldMinX>(table, index),
        TableOperations::_unwrapRowWithOffset<SweepNPruneBroadphase::OldMaxX>(table, index),
        TableOperations::_unwrapRowWithOffset<SweepNPruneBroadphase::NewMinX>(table, index),
        TableOperations::_unwrapRowWithOffset<SweepNPruneBroadphase::NewMaxX>(table, index),
        TableOperations::_unwrapRowWithOffset<FloatRow<Tags::Pos, Tags::X>>(table, index),
        {},
        needsReinsert);
      SweepNPruneBroadphase::recomputeBoundaries(
        TableOperations::_unwrapRowWithOffset<SweepNPruneBroadphase::OldMinY>(table, index),
        TableOperations::_unwrapRowWithOffset<SweepNPruneBroadphase::OldMaxY>(table, index),
        TableOperations::_unwrapRowWithOffset<SweepNPruneBroadphase::NewMinY>(table, index),
        TableOperations::_unwrapRowWithOffset<SweepNPruneBroadphase::NewMaxY>(table, index),
        TableOperations::_unwrapRowWithOffset<FloatRow<Tags::Pos, Tags::Y>>(table, index),
        {},
        needsReinsert);
    }

    template<class TableT>
    static void _insertBroadphase(size_t index, TableT& table, BroadphaseTable& broadphase) {
      _computeBroadphaseBoundaries(index, table);
      SweepNPruneBroadphase::insertRange(index, 1,
        broadphase,
        std::get<SweepNPruneBroadphase::OldMinX>(table.mRows),
        std::get<SweepNPruneBroadphase::OldMinY>(table.mRows),
        std::get<SweepNPruneBroadphase::OldMaxX>(table.mRows),
        std::get<SweepNPruneBroadphase::OldMaxY>(table.mRows),
        std::get<SweepNPruneBroadphase::NewMinX>(table.mRows),
        std::get<SweepNPruneBroadphase::NewMinY>(table.mRows),
        std::get<SweepNPruneBroadphase::NewMaxX>(table.mRows),
        std::get<SweepNPruneBroadphase::NewMaxY>(table.mRows),
        std::get<SweepNPruneBroadphase::Key>(table.mRows));
    }

    static void _updateCollisionPairs(GameDatabase& db) {
      BroadphaseTable& broadphase = std::get<BroadphaseTable>(db.mTables);
      auto& changes = std::get<SharedRow<SweepNPruneBroadphase::PairChanges>>(broadphase.mRows).at();
      auto& mappings = std::get<SharedRow<SweepNPruneBroadphase::CollisionPairMappings>>(broadphase.mRows).at();
      SweepNPruneBroadphase::ChangedCollisionPairs resultChanges;
      SweepNPruneBroadphase::updateCollisionPairs<CollisionPairIndexA, CollisionPairIndexB, GameDatabase>
        (changes, mappings, std::get<CollisionPairsTable>(db.mTables), PhysicsSimulation::_getPhysicsTableIds(), _getStableMappings(db), resultChanges);
    }

    template<class TableT>
    static void _insertBroadphaseAndFillNarrowphase(GameDatabase& db, TableT& table, std::initializer_list<size_t> ids) {
      for(size_t id : ids) {
        _insertBroadphase(id, table, std::get<BroadphaseTable>(db.mTables));
      }
      _updateCollisionPairs(db);
      _fillNarrowphaseData(db);
    }

    template<class TableT>
    static void _insertBroadphaseAndFillNarrowphaseAndContacts(GameDatabase& db, TableT& table, std::initializer_list<size_t> ids) {
      _insertBroadphaseAndFillNarrowphase(db, table, ids);
      Physics::generateContacts(std::get<CollisionPairsTable>(db.mTables));
    }

    static PhysicsConfig _configWithPadding(size_t padding) {
      PhysicsConfig result;
      result.mForcedTargetWidth = padding;
      return result;
    }

    static void _buildConstraintsTable(GameDatabase& db, const PhysicsConfig& config) {
      DBReader reader(db);
      _execute(ConstraintsTableBuilder::build(db, reader.mChangedPairs, reader.mStableMappings, reader.mConstraintsMappings, PhysicsSimulation::_getPhysicsTableIds(), config));
    }

    TEST_METHOD(CollidingPair_PopulateNarrowphase_IsPopulated) {
      GameDatabase db;
      auto& gameobjects = std::get<GameObjectTable>(db.mTables);
      TableOperations::stableResizeTable(gameobjects, UnpackedDatabaseElementID::fromPacked(GameDatabase::getTableIndex<GameObjectTable>()), 2, _getStableMappings(db));
      auto& posX = std::get<FloatRow<Tags::Pos, Tags::X>>(gameobjects.mRows);
      posX.at(0) = 1.1f;
      posX.at(1) = 1.2f;
      auto& pairs = std::get<CollisionPairsTable>(db.mTables);

      _insertBroadphaseAndFillNarrowphase(db, gameobjects, { 0, 1 });

      Assert::AreEqual(size_t(1), TableOperations::size(pairs));
      auto& narrowphasePosX = std::get<NarrowphaseData<PairA>::PosX>(pairs.mRows);
      //Don't really care what order it copied in as long as the values were copied from gameobjects to narrowphase
      Assert::IsTrue(narrowphasePosX.at(0) == 1.1f || narrowphasePosX.at(0) == 1.2f);
    }

    TEST_METHOD(DistantPair_PopulateNarrowphase_NoPairs) {
      GameDatabase db;
      auto& gameobjects = std::get<GameObjectTable>(db.mTables);
      TableOperations::stableResizeTable(gameobjects, UnpackedDatabaseElementID::fromPacked(GameDatabase::getTableIndex<GameObjectTable>()), 2, _getStableMappings(db));
      auto& posX = std::get<FloatRow<Tags::Pos, Tags::X>>(gameobjects.mRows);
      auto& pairs = std::get<CollisionPairsTable>(db.mTables);
      posX.at(0) = 1.0f;
      posX.at(1) = 5.0f;

      _insertBroadphaseAndFillNarrowphase(db, gameobjects, { 0, 1 });

      Assert::AreEqual(size_t(0), TableOperations::size(pairs));
    }

    TEST_METHOD(TwoPairsSameObject_GenerateCollisionPairs_HasPairs) {
      GameDatabase db;
      auto& gameobjects = std::get<GameObjectTable>(db.mTables);
      TableOperations::stableResizeTable(gameobjects, UnpackedDatabaseElementID::fromPacked(GameDatabase::getTableIndex<GameObjectTable>()), 3, _getStableMappings(db));

      auto& posX = std::get<FloatRow<Tags::Pos, Tags::X>>(gameobjects.mRows);
      auto& pairs = std::get<CollisionPairsTable>(db.mTables);
      //This one to collide with both
      posX.at(0) = 5.0f;
      //This one to the left to collide with 1 but not 2
      posX.at(1) = 4.0f;
      //To the right, colliding with 0 but not 1
      posX.at(2) = 6.0f + SweepNPruneBroadphase::BoundariesConfig{}.mPadding;

      _insertBroadphaseAndFillNarrowphase(db, gameobjects, { 0, 1, 2 });

      Assert::AreEqual(size_t(2), TableOperations::size(pairs));
      std::pair<size_t, size_t> a, b;
      auto pairA = TableOperations::getElement(pairs, 0);
      auto pairB = TableOperations::getElement(pairs, 1);
      //These asserts are enforcing a result order but that's not a requirement, rather it's easier to write the test that way
      const size_t tableIndex = GameDatabase::getTableIndex<GameObjectTable>().mValue;
      Assert::IsTrue(StableElementID{ tableIndex + size_t(0), 1 } == pairA.get<0>());
      Assert::IsTrue(StableElementID{ tableIndex + size_t(1), 2 } == pairA.get<1>());

      Assert::IsTrue(StableElementID{ tableIndex + size_t(0), 1 } == pairB.get<0>());
      Assert::IsTrue(StableElementID{ tableIndex + size_t(2), 3 } == pairB.get<1>());
    }

    TEST_METHOD(CollidingPair_GenerateContacts_AreGenerated) {
      GameDatabase db;
      auto& gameobjects = std::get<GameObjectTable>(db.mTables);
      TableOperations::stableResizeTable(gameobjects, UnpackedDatabaseElementID::fromPacked(GameDatabase::getTableIndex<GameObjectTable>()), 2, _getStableMappings(db));
      auto& posX = std::get<FloatRow<Tags::Pos, Tags::X>>(gameobjects.mRows);
      auto& cosAngle = std::get<FloatRow<Tags::Rot, Tags::CosAngle>>(gameobjects.mRows);
      cosAngle.at(0) = 1.0f;
      cosAngle.at(1) = 1.0f;
      auto& pairs = std::get<CollisionPairsTable>(db.mTables);
      const float expectedOverlap = 0.1f;
      posX.at(0) = 5.0f;
      posX.at(1) = 6.0f - expectedOverlap;

      _insertBroadphaseAndFillNarrowphase(db, gameobjects, { 0, 1 });

      Physics::generateContacts(pairs);

      Assert::AreEqual(size_t(1), TableOperations::size(pairs));
      const float e = 0.00001f;
      Assert::AreEqual(expectedOverlap, std::get<ContactPoint<ContactOne>::Overlap>(pairs.mRows).at(0), e);
      Assert::AreEqual(5.5f - expectedOverlap, std::get<ContactPoint<ContactOne>::PosX>(pairs.mRows).at(0), e);
      Assert::AreEqual(-1.0f, std::get<SharedNormal::X>(pairs.mRows).at(0), e);
    }

    TEST_METHOD(CollidingPair_SolveConstraints_AreSeparated) {
      GameDatabase db;
      auto& gameobjects = std::get<GameObjectTable>(db.mTables);
      TableOperations::stableResizeTable(gameobjects, UnpackedDatabaseElementID::fromPacked(GameDatabase::getTableIndex<GameObjectTable>()), 2, _getStableMappings(db));
      auto& constraints = std::get<ConstraintsTable>(db.mTables);
      auto& staticConstraints = std::get<ContactConstraintsToStaticObjectsTable>(db.mTables);
      auto& commonConstraints = std::get<ConstraintCommonTable>(db.mTables);
      auto& cosAngle = std::get<FloatRow<Tags::Rot, Tags::CosAngle>>(gameobjects.mRows);
      cosAngle.at(0) = 1.0f;
      cosAngle.at(1) = 1.0f;
      auto& posX = std::get<FloatRow<Tags::Pos, Tags::X>>(gameobjects.mRows);
      auto& velX = std::get<FloatRow<Tags::LinVel, Tags::X>>(gameobjects.mRows);
      auto& angVel = std::get<FloatRow<Tags::AngVel, Tags::Angle>>(gameobjects.mRows);
      const float expectedOverlap = 0.1f;
      posX.at(0) = 5.0f;
      velX.at(0) = 1.0f;
      posX.at(1) = 6.0f - expectedOverlap;
      velX.at(1) = -1.0f;

      _insertBroadphaseAndFillNarrowphaseAndContacts(db, gameobjects, { 0, 1 });

      _buildConstraintsTable(db, _configWithPadding(0));
      _fillConstraintVelocities(db);

      Physics::setupConstraints(constraints, staticConstraints);
      for(int i = 0; i < 5; ++i) {
        Physics::solveConstraints(constraints, staticConstraints, commonConstraints, {});
      }
      _storeConstraintVelocities(db);
      const glm::vec2 centerAToContact{ 0.4f, 0.5f };
      const glm::vec2 centerBToContact{ -0.5f, 0.5f };
      const glm::vec2 normal{ -1.0f, 0.0f };
      //Velocity of contact point as velocity of center plus the angular component which is angular velocity cross the center to contact projected onto the x axis
      const float xVelocityOfAAtContactA = velX.at(0) - angVel.at(0)*centerAToContact.y;
      const float xVelocityOfBAtContactA = velX.at(1) - angVel.at(1)*centerBToContact.y;
      const float velocityDifference = xVelocityOfAAtContactA + xVelocityOfBAtContactA;

      //Need to be pretty loose on the comparison because friction makes the rotation part not completely zero
      const float e = 0.01f;
      //Constraint is trying to solve for the difference of the projection of the velocities of the contact point on A and B on the normal being zero
      Assert::AreEqual(0.0f, velocityDifference, e);
    }

    struct SweepEntry {
      glm::vec2 mNewBoundaryMin{};
      glm::vec2 mNewBoundaryMax{};
      glm::vec2 mOldBoundaryMin{};
      size_t mKey{};
    };

    static void _clear(std::vector<SweepCollisionPair>& a, std::vector<SweepCollisionPair>& b) {
      a.clear();
      b.clear();
    }

    static void _insertOne(Sweep2D& sweep, SweepEntry& entry, std::vector<SweepCollisionPair>& gained) {
      SweepNPrune::insertRange(sweep, &entry.mNewBoundaryMin.x, &entry.mNewBoundaryMin.y, &entry.mNewBoundaryMax.x, &entry.mNewBoundaryMax.y, &entry.mKey, gained, size_t(1));
      entry.mOldBoundaryMin = entry.mNewBoundaryMin;
    }

    static void _eraseOne(Sweep2D& sweep, SweepEntry& entry, std::vector<SweepCollisionPair>& lost) {
      SweepNPrune::eraseRange(sweep, &entry.mOldBoundaryMin.x, &entry.mOldBoundaryMin.y, &entry.mKey, lost, 1);
    }

    static void _reinsertOne(Sweep2D& sweep, SweepEntry& entry, std::vector<SweepCollisionPair>& gained, std::vector<SweepCollisionPair>& lost) {
      SweepNPrune::reinsertRange(sweep,
        &entry.mOldBoundaryMin.x,
        &entry.mOldBoundaryMin.y,
        &entry.mNewBoundaryMin.x,
        &entry.mNewBoundaryMin.y,
        &entry.mNewBoundaryMax.x,
        &entry.mNewBoundaryMax.y,
        &entry.mKey,
        gained,
        lost,
        1);
      entry.mOldBoundaryMin = entry.mNewBoundaryMin;
    }

    static bool pairMatches(const SweepCollisionPair& li, const SweepCollisionPair& ri) {
      SweepCollisionPair l = li;
      SweepCollisionPair r = ri;
      if(l.mA > l.mB) {
        std::swap(l.mA, l.mB);
      }
      if(r.mA > r.mB) {
        std::swap(r.mA, r.mB);
      }
      return r.mA == l.mA && r.mB == l.mB;
    }

    static void assertPairsMatch(const std::vector<SweepCollisionPair>& l, std::initializer_list<SweepCollisionPair> r) {
      Assert::AreEqual(l.size(), r.size());
      //Order doesn't matter
      for(auto it = l.begin(); it != l.end(); ++it) {
        Assert::IsTrue(std::any_of(r.begin(), r.end(), [&it](const SweepCollisionPair& r) { return pairMatches(*it, r); }));
      }
    }

    TEST_METHOD(SweepNPrune_EraseOneOverlappingAxis_NoLoss) {
      Sweep2D sweep;
      std::vector<SweepCollisionPair> pairs;
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

    TEST_METHOD(SweepNPrune_InsertRange) {
      Sweep2D sweep;
      std::vector<SweepCollisionPair> pairs;
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
      assertPairsMatch(pairs, { SweepCollisionPair{ 1, 2 } });
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
      assertPairsMatch(pairs, { SweepCollisionPair{ 5, 1 }, SweepCollisionPair{ 5, 2 }, SweepCollisionPair{ 5, 3 } });
      pairs.clear();

      //Entirely containing right
      SweepEntry rightOverlap;
      rightOverlap.mNewBoundaryMin = right.mNewBoundaryMin - glm::vec2(0.1f);
      rightOverlap.mNewBoundaryMax = right.mNewBoundaryMax + glm::vec2(0.1f);
      rightOverlap.mKey = 6;
      _insertOne(sweep, rightOverlap, pairs);
      assertPairsMatch(pairs, { SweepCollisionPair{ 6, 4 } });
      pairs.clear();

      //Contained by right and rightOVerlap
      SweepEntry rightContained;
      rightContained.mNewBoundaryMin = right.mNewBoundaryMin + glm::vec2(0.1f);
      rightContained.mNewBoundaryMax = rightContained.mNewBoundaryMin + glm::vec2(0.1f);
      rightContained.mKey = 7;
      _insertOne(sweep, rightContained, pairs);
      assertPairsMatch(pairs, { SweepCollisionPair{ 7, 4 }, SweepCollisionPair{ 7, 6 } });
      pairs.clear();

      std::vector<SweepCollisionPair> lostPairs;
      _eraseOne(sweep, rightContained, lostPairs);
      assertPairsMatch(lostPairs, { SweepCollisionPair{ 7, 4 }, SweepCollisionPair{ 7, 6 } });
      lostPairs.clear();

      _eraseOne(sweep, rightOverlap, lostPairs);
      assertPairsMatch(lostPairs, { SweepCollisionPair{ 6, 4 } });
      lostPairs.clear();

      _eraseOne(sweep, leftToCenter, lostPairs);
      assertPairsMatch(lostPairs, { SweepCollisionPair{ 5, 1 }, SweepCollisionPair{ 5, 2 }, SweepCollisionPair{ 5, 3 } });
      lostPairs.clear();

      _eraseOne(sweep, right, lostPairs);
      assertPairsMatch(lostPairs, {});
      lostPairs.clear();

      _eraseOne(sweep, left, lostPairs);
      assertPairsMatch(lostPairs, {});
      lostPairs.clear();

      _eraseOne(sweep, same, lostPairs);
      assertPairsMatch(lostPairs, { SweepCollisionPair{ 1, 2 } });
      lostPairs.clear();

      _eraseOne(sweep, entry, lostPairs);
      assertPairsMatch(lostPairs, {});
      lostPairs.clear();
    }

    TEST_METHOD(SweepNPrune_ReinsertRange) {
      Sweep2D sweep;
      std::vector<SweepCollisionPair> gainedPairs, lostPairs;
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
      assertPairsMatch(gainedPairs, { SweepCollisionPair{ 2, 1 }, SweepCollisionPair{ 2, 3 }, SweepCollisionPair{ 2, 4 } });
      assertPairsMatch(lostPairs, {});
      _clear(gainedPairs, lostPairs);

      //Move bottom left away from all
      bottomLeft.mNewBoundaryMax -= glm::vec2(100.0f);
      bottomLeft.mNewBoundaryMin -= glm::vec2(100.0f);
      _reinsertOne(sweep, bottomLeft, gainedPairs, lostPairs);
      assertPairsMatch(gainedPairs, {});
      assertPairsMatch(lostPairs, { SweepCollisionPair{ 3, 2 } });
      _clear(gainedPairs, lostPairs);

      //Undo the previous move
      bottomLeft.mNewBoundaryMax += glm::vec2(100.0f);
      bottomLeft.mNewBoundaryMin += glm::vec2(100.0f);
      _reinsertOne(sweep, bottomLeft, gainedPairs, lostPairs);
      assertPairsMatch(gainedPairs, { SweepCollisionPair{ 3, 2 } });
      assertPairsMatch(lostPairs, {});
      _clear(gainedPairs, lostPairs);

      //Move right to within lower right, but out of the left two
      upperRight.mNewBoundaryMin.x = bottomRight.mNewBoundaryMin.x + 0.1f;
      _reinsertOne(sweep, upperRight, gainedPairs, lostPairs);
      assertPairsMatch(gainedPairs, {});
      assertPairsMatch(lostPairs, { SweepCollisionPair{ 2, 1 }, SweepCollisionPair{ 2, 3 } });
      _clear(gainedPairs, lostPairs);

      //Restore right to how it started
      upperRight.mNewBoundaryMin = glm::vec2(size + space, size + space);
      upperRight.mNewBoundaryMax = upperRight.mNewBoundaryMin + glm::vec2(size);
      _reinsertOne(sweep, upperRight, gainedPairs, lostPairs);
      assertPairsMatch(gainedPairs, {});
      assertPairsMatch(lostPairs, { SweepCollisionPair{ 2, 4 } });
      _clear(gainedPairs, lostPairs);

      //Extend bottom left up into upper right, overlapping with everything
      bottomLeft.mNewBoundaryMax += glm::vec2(size * 0.5f);
      _reinsertOne(sweep, bottomLeft, gainedPairs, lostPairs);
      assertPairsMatch(gainedPairs, { SweepCollisionPair{ 3, 1 }, SweepCollisionPair{ 3, 2 }, SweepCollisionPair{ 3, 4 } });
      assertPairsMatch(lostPairs, {});
      _clear(gainedPairs, lostPairs);

      //Shrink it back on top so it's only overlapping with bottom right
      bottomLeft.mNewBoundaryMax.y = bottomRight.mNewBoundaryMax.y;
      _reinsertOne(sweep, bottomLeft, gainedPairs, lostPairs);
      assertPairsMatch(gainedPairs, {});
      assertPairsMatch(lostPairs, { SweepCollisionPair{ 3, 1 }, SweepCollisionPair{ 3, 2 } });
      _clear(gainedPairs, lostPairs);

      //Resize and move bottom left to inside of upperRight
      bottomLeft.mNewBoundaryMin = upperRight.mNewBoundaryMin + glm::vec2(0.1f);
      bottomLeft.mNewBoundaryMax = bottomLeft.mNewBoundaryMax + glm::vec2(0.1f);
      _reinsertOne(sweep, bottomLeft, gainedPairs, lostPairs);
      assertPairsMatch(gainedPairs, { SweepCollisionPair{ 3, 2 } });
      assertPairsMatch(lostPairs, { SweepCollisionPair{ 3, 4 } });
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

    static bool _unorderedIdsMatch(const CollisionPairIndexA& actualA, const CollisionPairIndexB& actualB,
      std::vector<StableElementID> expectedA,
      std::vector<StableElementID> expectedB,
      const ConstraintData::IsEnabled* isEnabled) {
      Assert::AreEqual(expectedA.size(), expectedB.size());

      for(size_t i = 0; i < actualA.size(); ++i) {
        if(isEnabled && !isEnabled->at(i)) {
          continue;
        }
        const StableElementID& toFindA = actualA.at(i);
        const StableElementID& toFindB = actualB.at(i);
        bool found = false;
        for(size_t j = 0; j < expectedA.size(); ++j) {
          if(toFindA == expectedA[j] && toFindB == expectedB[j]) {
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

    static bool _unorderedCollisionPairsMatch(DBReader& db, std::vector<StableElementID> expectedA, std::vector<StableElementID> expectedB) {
      return _unorderedIdsMatch(db.mCollisionPairA, db.mCollisionPairB, expectedA, expectedB, nullptr) &&
        _unorderedIdsMatch(db.mConstraintPairA, db.mConstraintPairB, expectedA, expectedB, &db.mIsConstraintEnabled);
    }

    static void _updatePhysics(GameDatabase& db) {
      PhysicsConfig config;
      config.mForcedTargetWidth = size_t(1);
      _execute(PhysicsSimulation::updatePhysics({ db }));
    }

    static void _assertEnabledContactConstraintCount(GameDatabase& db, size_t expected) {
      DBReader reader(db);
      const size_t contactCount = reader.mConstraintsMappings.mZeroMassStartIndex;
      size_t enabled = 0;
      for(size_t i = 0; i < contactCount; ++i) {
        if(reader.mIsConstraintEnabled.at(i)) {
          ++enabled;
        }
      }
      Assert::AreEqual(expected, enabled);
      Assert::AreEqual(contactCount, TableOperations::size(reader.mConstraints));
    }

    static void _assertEnabledStaticContactConstraintCount(GameDatabase& db, size_t expected) {
      DBReader reader(db);
      const size_t staticCount = TableOperations::size(reader.mConstraintsCommon) - reader.mConstraintsMappings.mZeroMassStartIndex;
      size_t enabled = 0;
      for(size_t i = 0; i < staticCount; ++i) {
        if(reader.mIsConstraintEnabled.at(i + reader.mConstraintsMappings.mZeroMassStartIndex)) {
          ++enabled;
        }
      }
      Assert::AreEqual(expected, enabled);
      Assert::AreEqual(staticCount, TableOperations::size(reader.mStaticConstraints));
    }

    static size_t _getConstraintIndexFromContact(GameDatabase& db, size_t contact) {
      DBReader reader(db);
      StableElementID id = reader.mContactPairedConstraint.at(contact);
      return StableOperations::tryResolveStableID(id, db, reader.mStableMappings)->mUnstableIndex & GameDatabase::ElementID::ELEMENT_INDEX_MASK;
    }

    TEST_METHOD(GameOneObject_Migrate_PhysicsDataPreserved) {
      GameDatabase db;
      Fragment::SceneArgs args{ 1, 1 };
      Fragment::_setupScene({ db }, args);
      DBReader reader(db);
      StableElementID playerId = StableOperations::getStableID(reader.mPlayerIDs, GameDatabase::getElementID<PlayerTable>(0));
      StableElementID objectId = StableOperations::getStableID(reader.mGameObjectIDs, GameDatabase::getElementID<GameObjectTable>(0));
      const size_t originalObjectStableId = objectId.mStableID;
      const float minCorrection = 0.1f;

      auto setInitialPos = [&] {
        reader.mPlayerPosX.at(0) = 1.5f;
        reader.mPlayerPosY.at(0) = 0.0f;
        reader.mPlayerLinVelX.at(0) = -0.5f;
        reader.mPosX.at(0) = 1.0f;
        reader.mPosY.at(0) = 0.0f;
        reader.mLinVelX.at(0) = 0.5f;
      };
      setInitialPos();
      _updatePhysics(db);

      auto assertInitialResolution = [&] {
        Assert::AreEqual(size_t(1), TableOperations::size(reader.mCollisionPairs));
        _assertEnabledContactConstraintCount(db, 1);
        _assertEnabledStaticContactConstraintCount(db, 0);
        Assert::IsTrue(reader.mCollisionPairA.at(0) == playerId);
        const size_t c = _getConstraintIndexFromContact(db, 0);
        Assert::IsTrue(reader.mConstraintPairA.at(c) == playerId);
        Assert::IsTrue(reader.mCollisionPairB.at(0) == objectId);
        Assert::IsTrue(reader.mConstraintPairB.at(c) == objectId);

        Assert::IsTrue(reader.mPlayerLinVelX.at(0) > -0.5f + minCorrection, L"Player should be pushed away from object");
        Assert::IsTrue(reader.mLinVelX.at(0) < 0.5f - minCorrection, L"Object should be pushed away from player");
      };
      assertInitialResolution();

      reader.mPlayerPosX.at(0) = 100.0f;
      _updatePhysics(db);

      auto assertNoPairs = [&] {
        Assert::AreEqual(size_t(0), TableOperations::size(reader.mCollisionPairs));
        _assertEnabledContactConstraintCount(db, 0);
        _assertEnabledStaticContactConstraintCount(db, 0);
      };
      assertNoPairs();

      setInitialPos();
      _updatePhysics(db);
      assertInitialResolution();

      std::get<FragmentGoalFoundRow>(reader.mGameObjects.mRows).at(0) = true;

      Fragment::_migrateCompletedFragments({ db }, 0);

      //Migrate will also snap the fragment to its goal, so recenter the player in collision with the new location
      auto setNewPos = [&] {
        reader.mPlayerPosX.at(0) = reader.mStaticPosX.at(0) + 0.5f;
        reader.mPlayerPosY.at(0) = reader.mStaticPosY.at(0);
      };
      setNewPos();

      //Object should have moved to the static table, and mapping updated to new unstable id pointing at static table but same stable id
      objectId = *StableOperations::tryResolveStableID(objectId, db, reader.mStableMappings);
      Assert::IsTrue(objectId == StableElementID{ GameDatabase::getElementID<StaticGameObjectTable>(0).mValue, originalObjectStableId });

      _updatePhysics(db);
      auto assertStaticCollision = [&] {
        //Similar to before, except now the single constraint is in the static table instead of the dynamic one
        //Pair order is the same, both because player has lower stable id (0) than object (1) but also because object is now static,
        //and static objects always order B in pairs
        Assert::AreEqual(size_t(1), TableOperations::size(reader.mCollisionPairs));
        _assertEnabledContactConstraintCount(db, 0);
        _assertEnabledStaticContactConstraintCount(db, 1);
        const size_t c = _getConstraintIndexFromContact(db, 0);
        Assert::IsTrue(reader.mCollisionPairA.at(0) == playerId);
        Assert::IsTrue(reader.mConstraintPairA.at(c) == playerId);
        Assert::IsTrue(reader.mCollisionPairB.at(0) == objectId);
        Assert::IsTrue(reader.mConstraintPairB.at(c) == objectId);

        Assert::IsTrue(reader.mPlayerLinVelX.at(0) > -0.5f, L"Player should be pushed away from object");
      };
      assertStaticCollision();

      reader.mPlayerPosX.at(0) = 100.0f;
      _updatePhysics(db);
      assertNoPairs();

      setNewPos();
      _updatePhysics(db);
      assertStaticCollision();
    }

    TEST_METHOD(GameTwoObjects_Migrate_PhysicsDataPreserved) {
      GameDatabase db;
      Fragment::SceneArgs args{ 1, 2 };
      Fragment::_setupScene({ db }, args);
      DBReader reader(db);
      StableElementID playerId = StableOperations::getStableID(reader.mPlayerIDs, GameDatabase::getElementID<PlayerTable>(0));
      StableElementID objectLeftId = StableOperations::getStableID(reader.mGameObjectIDs, GameDatabase::getElementID<GameObjectTable>(0));
      StableElementID objectRightId = StableOperations::getStableID(reader.mGameObjectIDs, GameDatabase::getElementID<GameObjectTable>(1));

      auto& goalX = std::get<FloatRow<Tags::FragmentGoal, Tags::X>>(reader.mGameObjects.mRows);
      auto& goalY = std::get<FloatRow<Tags::FragmentGoal, Tags::Y>>(reader.mGameObjects.mRows);
      glm::vec2 avg{ 0.0f };
      for(size_t i = 0; i < 2; ++i) {
        reader.mPosX.at(i) = goalX.at(i);
        reader.mPosY.at(i) = goalY.at(i);
        reader.mLinVelY.at(i) = 0.5f;
        avg.x += reader.mPosX.at(i);
        avg.y += reader.mPosY.at(i);
      }
      avg *= 0.5f;
      glm::vec2 initialRight{ reader.mPosX.at(1), reader.mPosY.at(1) };

      auto setInitialPlayerPos = [&] {
        reader.mPlayerPosX.at(0) = avg.x;
        reader.mPlayerPosY.at(0) = avg.y + 0.75f;
        reader.mPlayerLinVelY.at(0) = -0.5f;
      };
      setInitialPlayerPos();

      _updatePhysics(db);

      Assert::AreEqual(size_t(3), TableOperations::size(reader.mCollisionPairs));
      _assertEnabledContactConstraintCount(db, 3);
      _assertEnabledStaticContactConstraintCount(db, 0);
      Assert::IsTrue(_unorderedCollisionPairsMatch(reader,
        { playerId, playerId, objectLeftId },
        { objectLeftId, objectRightId, objectRightId }
      ));

      //Min ia a bit weirder here since the impulse is spread between the two objects and at an angle
      const float minCorrection = 0.05f;
      Assert::IsTrue(reader.mPlayerLinVelY.at(0) > -0.5f + minCorrection, L"Player should be pushed away from object");
      Assert::IsTrue(reader.mLinVelY.at(0) < 0.5f - minCorrection, L"Object should be pushed away from player");
      Assert::IsTrue(reader.mLinVelY.at(1) < 0.5f - minCorrection, L"Object should be pushed away from player");

      std::get<FragmentGoalFoundRow>(reader.mGameObjects.mRows).at(0) = true;
      Fragment::_migrateCompletedFragments({ db }, 0);

      //Need to update both since one moved and the other was affected by swap removal
      objectLeftId = *StableOperations::tryResolveStableID(objectLeftId, db, reader.mStableMappings);
      objectRightId = *StableOperations::tryResolveStableID(objectRightId, db, reader.mStableMappings);

      auto resetStaticPos = [&] {
        setInitialPlayerPos();
        reader.mPosX.at(0) = initialRight.x;
        reader.mPosY.at(0) = initialRight.y + 0.1f;
        reader.mLinVelY.at(0) = 0.5f;
      };

      resetStaticPos();
      _updatePhysics(db);

      auto assertStaticCollision = [&] {
      Assert::AreEqual(size_t(3), TableOperations::size(reader.mCollisionPairs));
      _assertEnabledContactConstraintCount(db, 1);
      _assertEnabledStaticContactConstraintCount(db, 2);
      //Now left is the B object always since it's static
      Assert::IsTrue(_unorderedCollisionPairsMatch(reader,
        { playerId, playerId, objectRightId },
        { objectLeftId, objectRightId, objectLeftId }
      ));

      Assert::IsTrue(reader.mPlayerLinVelY.at(0) > -0.5f + minCorrection, L"Player should be pushed away from object");
      Assert::IsTrue(reader.mLinVelY.at(0) < 0.5f - minCorrection, L"Object should be pushed away from player");
      };
      assertStaticCollision();

      resetStaticPos();
      reader.mPlayerPosX.at(0) = 100.0f;
      _updatePhysics(db);

      Assert::AreEqual(size_t(1), TableOperations::size(reader.mCollisionPairs));
      _assertEnabledContactConstraintCount(db, 0);
      _assertEnabledStaticContactConstraintCount(db, 1);
      Assert::IsTrue(_unorderedCollisionPairsMatch(reader,
        { objectRightId },
        { objectLeftId }
      ));

      resetStaticPos();
      _updatePhysics(db);

      assertStaticCollision();
    }

    template<class TableT>
    static constexpr UnpackedDatabaseElementID _getUnpackedID(size_t index = 0) {
      return UnpackedDatabaseElementID::fromPacked(GameDatabase::getElementID<TableT>(index));
    }

    template<class TableT>
    static void _stableResize(TableT& table, size_t newSize, StableElementMappings& mappings) {
      TableOperations::stableResizeTable(table, _getUnpackedID<TableT>(), newSize, mappings);
    }

    static StableElementID _addCollisionPair(GameDatabase& db, StableElementID a, StableElementID b) {
      DBReader reader(db);
      const size_t oldSize = TableOperations::size(reader.mCollisionPairs);
      _stableResize(reader.mCollisionPairs, oldSize + 1, reader.mStableMappings);
      reader.mCollisionPairA.at(oldSize) = a;
      reader.mCollisionPairB.at(oldSize) = b;
      StableElementID result = StableOperations::getStableID(reader.mCollisionPairIDs, _getUnpackedID<CollisionPairsTable>(oldSize));

      reader.mChangedPairs.mGained.push_back(result);

      return result;
    }

    static StableElementID _getGameobjectID(GameDatabase& db, size_t index) {
      DBReader reader(db);
      return StableOperations::getStableID(reader.mGameObjectIDs, _getUnpackedID<GameObjectTable>(index));
    }

    //Hack to match ispc enum
    static constexpr int NO_SYNC = 0;
    static constexpr int SYNC_TO_A = 1;
    static constexpr int SYNC_TO_B = 2;

    TEST_METHOD(ConstraintsTableBuilder_Integration) {
      GameDatabase db;
      DBReader reader(db);
      TableOperations::stableResizeTable(reader.mGameObjects, UnpackedDatabaseElementID::fromPacked(GameDatabase::getTableIndex<GameObjectTable>()), 5, reader.mStableMappings);
      const PhysicsConfig config = _configWithPadding(1);

      StableElementID a = _getGameobjectID(db, 0);
      StableElementID b = _getGameobjectID(db, 1);
      StableElementID pair = _addCollisionPair(db, a, b);

      _buildConstraintsTable(db, config);

      Assert::AreEqual(size_t(2), TableOperations::size(reader.mConstraintsCommon), L"Should have one real element and one padding");
      Assert::AreEqual(NO_SYNC, reader.mSyncTypeA.at(0));
      Assert::AreEqual(NO_SYNC, reader.mSyncTypeB.at(0));
    }

    static auto getTargetElementRange(GameDatabase& db, size_t tableId) {
      DBReader reader(db);
      return ctbdetails::getTargetElementRange(tableId, PhysicsSimulation::_getPhysicsTableIds(), reader.mConstraintsMappings, TableOperations::size(reader.mConstraintsCommon));
    }

    static StableElementID tryTakeSuitableFreeSlot(GameDatabase& db, size_t tableId, const StableElementID& a, const StableElementID& b, size_t width) {
      DBReader reader(db);
      StableElementID result = ctbdetails::tryTakeSuitableFreeSlot(0,
        tableId,
        getTargetElementRange(db, tableId),
        reader.mConstraintsMappings,
        reader.mStableMappings,
        PhysicsSimulation::_getPhysicsTableIds(),
        a,
        b,
        reader.mConstraintPairA,
        reader.mConstraintPairB,
        reader.mConstraintIDs,
        width);

      if(result != StableElementID::invalid()) {
        StableElementID p = _addCollisionPair(db, a, b);
        ctbdetails::assignConstraint(p, a, b, result, reader.mCollisionPairs, reader.mConstraintsCommon, PhysicsSimulation::_getPhysicsTableIds());
      }
      return result;
    }

    void addPaddingToTable(GameDatabase& db, size_t table, size_t amount) {
      DBReader reader(db);
      ctbdetails::addPaddingToTable(table, amount, reader.mConstraintsCommon, reader.mStableMappings, PhysicsSimulation::_getPhysicsTableIds(), reader.mConstraintsMappings);
    }

    TEST_METHOD(CTBDetails) {
      GameDatabase db;
      DBReader reader(db);
      const PhysicsTableIds tableIds = PhysicsSimulation::_getPhysicsTableIds();
      const PhysicsConfig config = _configWithPadding(2);

      _stableResize(reader.mGameObjects, 5, reader.mStableMappings);
      StableElementID a = _getGameobjectID(db, 0);
      StableElementID b  = _getGameobjectID(db, 1);
      StableElementID c = _getGameobjectID(db, 2);
      StableElementID d = _getGameobjectID(db, 3);

      auto range = getTargetElementRange(db, tableIds.mSharedMassConstraintTable);
      Assert::IsTrue(range == std::make_pair(size_t(0), size_t(0)));
      range = getTargetElementRange(db, tableIds.mZeroMassConstraintTable);
      Assert::IsTrue(range == std::make_pair(size_t(0), size_t(0)));

      //Should fail because nothing is there
      Assert::IsTrue(StableElementID::invalid() == tryTakeSuitableFreeSlot(db, tableIds.mSharedMassConstraintTable, a, b, *config.mForcedTargetWidth));
      Assert::IsTrue(StableElementID::invalid() == tryTakeSuitableFreeSlot(db, tableIds.mZeroMassConstraintTable, a, b, *config.mForcedTargetWidth));

      addPaddingToTable(db, tableIds.mZeroMassConstraintTable, 1);

      //Should fail because the space was added to the zero mass table
      Assert::IsTrue(StableElementID::invalid() == tryTakeSuitableFreeSlot(db, tableIds.mSharedMassConstraintTable, a, b, *config.mForcedTargetWidth));
      StableElementID ab = tryTakeSuitableFreeSlot(db, tableIds.mZeroMassConstraintTable, a, b, *config.mForcedTargetWidth);
      Assert::AreEqual(size_t(0), ab.mUnstableIndex & tableIds.mElementIDMask);

      addPaddingToTable(db, tableIds.mZeroMassConstraintTable, 1);

      Assert::IsTrue(StableElementID::invalid() == tryTakeSuitableFreeSlot(db, tableIds.mZeroMassConstraintTable, a, c, *config.mForcedTargetWidth), L"Should not accept slots within target width of non-static object");
      StableElementID cb = tryTakeSuitableFreeSlot(db, tableIds.mZeroMassConstraintTable, c, b, *config.mForcedTargetWidth);
      Assert::AreEqual(size_t(1), cb.mUnstableIndex & tableIds.mElementIDMask, L"Static elements should ignore padding rules");

      addPaddingToTable(db, tableIds.mSharedMassConstraintTable, 1);

      StableElementID ad = tryTakeSuitableFreeSlot(db, tableIds.mSharedMassConstraintTable, a, d, *config.mForcedTargetWidth);
      Assert::AreEqual(size_t(0), ad.mUnstableIndex & tableIds.mElementIDMask, L"Elements in dynamic section should ignore padding rules in neighboring sections");

      addPaddingToTable(db, tableIds.mSharedMassConstraintTable, 1);

      Assert::IsTrue(StableElementID::invalid() == tryTakeSuitableFreeSlot(db, tableIds.mSharedMassConstraintTable, a, b, *config.mForcedTargetWidth), L"Object A should obey padding rules");
      Assert::IsTrue(StableElementID::invalid() == tryTakeSuitableFreeSlot(db, tableIds.mSharedMassConstraintTable, b, d, *config.mForcedTargetWidth), L"Object B should obey padding rules");
      StableElementID bc = tryTakeSuitableFreeSlot(db, tableIds.mSharedMassConstraintTable, b, c, *config.mForcedTargetWidth);
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

    TEST_METHOD(Stats) {
      GameArgs args;
      constexpr size_t OBJ_COUNT = 3;
      args.fragmentCount = OBJ_COUNT;
      TestGame game{ args };
      auto stats = TableAdapters::getThreadLocal(game, 0).statEffects;
      auto& lambdaStat = std::get<LambdaStatEffectTable>(stats->db.mTables);
      auto& posStat = std::get<PositionStatEffectTable>(stats->db.mTables);
      auto& velStat = std::get<VelocityStatEffectTable>(stats->db.mTables);
      LambdaStatEffectAdapter lambda = TableAdapters::getLambdaEffects(game, 0);
      PositionStatEffectAdapter pos = TableAdapters::getPositionEffects(game, 0);
      VelocityStatEffectAdapter vel = TableAdapters::getVelocityEffects(game, 0);
      ThreadLocalData tls = TableAdapters::getThreadLocal(game, 0);
      GameObjectAdapter gameobjects = TableAdapters::getGameObjects(game);
      StableIDRow& objsStable = *gameobjects.stable;
      auto tableBase = UnpackedDatabaseElementID::fromPacked(GameDatabase::getTableIndex<GameObjectTable>());

      for(size_t i = 0; i < OBJ_COUNT; ++i) {
        //Need to move them away from the fragment completion location
        gameobjects.transform.posX->at(i) = 4.0f;
      }

      StatEffect::addEffects(1, lambdaStat, tls.statEffects->db);
      lambda.base.lifetime->at(0) = 1;
      lambda.base.owner->at(0) = StableOperations::getStableID(objsStable, tableBase.remakeElement(0));
      int lambdaInvocations = 0;
      lambda.command->at(0) = [&](LambdaStatEffect::Args& args) {
        Assert::AreEqual(size_t(0), args.resolvedID.toPacked<GameDatabase>().getElementIndex());
        ++lambdaInvocations;
      };

      StatEffect::addEffects(1, velStat, tls.statEffects->db);
      vel.base.lifetime->at(0) = 2;
      vel.base.owner->at(0) = StableOperations::getStableID(objsStable, tableBase.remakeElement(1));
      VelocityStatEffect::VelocityCommand& vcmd = vel.command->at(0);
      vcmd.linearImpulse = glm::vec2(1.0f);
      vcmd.angularImpulse = 1.0f;

      StatEffect::addEffects(1, posStat, tls.statEffects->db);
      pos.base.lifetime->at(0) = 1;
      pos.base.owner->at(0) = StableOperations::getStableID(objsStable, tableBase.remakeElement(2));
      PositionStatEffect::PositionCommand& pcmd = pos.command->at(0);
      pcmd.pos = glm::vec2(5.0f);
      pcmd.rot = glm::vec2(0.0f, 1.0f);

      auto globalStats = TableAdapters::getCentralStatEffects(game);
      auto& lambdaLifetime = *globalStats.lambda.base.lifetime;
      auto& velLifetime = *globalStats.velocity.base.lifetime;
      auto& posLifetime = *globalStats.position.base.lifetime;

      game.update();

      Assert::AreEqual(1, lambdaInvocations);
      Assert::AreEqual(size_t(1), lambdaLifetime.size());
      Assert::AreEqual(size_t(0), lambdaLifetime.at(0));

      Assert::AreEqual(1.0f, gameobjects.physics.linVelX->at(1), 0.1f);
      Assert::AreEqual(1.0f, gameobjects.physics.linVelY->at(1), 0.1f);
      Assert::AreEqual(1.0f, gameobjects.physics.angVel->at(1), 0.1f);
      Assert::AreEqual(size_t(1), velLifetime.size());
      Assert::AreEqual(size_t(1), velLifetime.at(0));

      Assert::AreEqual(5.0f, gameobjects.transform.posX->at(2));
      Assert::AreEqual(5.0f, gameobjects.transform.posY->at(2));
      Assert::AreEqual(0.0f, gameobjects.transform.rotX->at(2));
      Assert::AreEqual(1.0f, gameobjects.transform.rotY->at(2));
      Assert::AreEqual(size_t(1), posLifetime.size());
      Assert::AreEqual(size_t(0), posLifetime.at(0));
    }

    TEST_METHOD(PlayerInput) {
      GameArgs args;
      args.playerPos = glm::vec2{ 0.0f };
      TestGame game{ args };
      PlayerAdapter player = TableAdapters::getPlayer(game);
      player.input->at(0).mMoveX = 1.0f;

      //Once to compute the impulse, next frame updates position with it
      game.update();
      game.update();

      Assert::IsTrue(player.object.transform.posX->at(0) > 0.0f);
    }

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

      AreaForceStatEffectAdapter effect = TableAdapters::getAreaForceEffects(game, 0);
      TableAdapters::addStatEffectsSharedLifetime(effect.base, StatEffect::INSTANT, nullptr, 1);
      effect.pointX->at(0) = 4.0f;
      effect.strength->at(0) = 10.0f;

      game.update();

      Assert::IsTrue(fragment.physics.linVelX->at(0) > 0.0f);
      Assert::AreEqual(size_t(0), effect.strength->size());
    }

    TEST_METHOD(Config) {
      GameConfig gameConfig;
      gameConfig.fragment.fragmentColumns = 5;

      std::string serialized = ConfigIO::serializeJSON(ConfigConvert::toConfig(gameConfig));
      ConfigIO::Result r = ConfigIO::deserializeJson(serialized);
      ConfigIO::Result r2 = ConfigIO::deserializeJson("test");

      Assert::AreEqual(size_t(0), r.value.index(), L"Parse should succeed");
      Assert::AreEqual(size_t(1), r2.value.index(), L"Parse should fail");
      Assert::AreEqual(size_t(5), std::get<0>(r.value).fragment.fragmentColumns);
    }
  };
}