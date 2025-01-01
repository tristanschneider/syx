#include "Precompile.h"
#include "CppUnitTest.h"

#include "config/ConfigIO.h"
#include "stat/AllStatEffects.h"
#include "Physics.h"
#include "Simulation.h"
#include "SweepNPrune.h"
#include "Table.h"

#include "PhysicsSimulation.h"
#include "Scheduler.h"
#include "StableElementID.h"
#include "TableAdapters.h"
#include "ThreadLocals.h"
#include "Fragment.h"
#include "Player.h"
#include "DBEvents.h"

#include "GameBuilder.h"
#include "GameScheduler.h"
#include "RuntimeDatabase.h"
#include "SweepNPruneBroadphase.h"
#include "SpatialQueries.h"
#include "FragmentStateMachine.h"
#include "SpatialPairsStorage.h"
#include "IslandGraph.h"
#include "ConstraintSolver.h"
#include "GameInput.h"
#include "scenes/SceneList.h"
#include "SceneNavigator.h"
#include "TestGame.h"

#include "stat/LambdaStatEffect.h"
#include "stat/VelocityStatEffect.h"
#include "stat/PositionStatEffect.h"
#include "stat/AreaForceStatEffect.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Test {
  using namespace Config;

  TEST_CLASS(GameSchedulerTest) {
    struct TestDB {
      struct TestDBT : Database<
        Table<Row<int>, Row<float>, StableIDRow>,
        Table<Row<int64_t>, StableIDRow>
      > {};

      TestDB() {
        db = createDB();
        scheduler.mScheduler.Initialize();
        tls = std::make_unique<ThreadLocals>(
          scheduler.mScheduler.GetNumTaskThreads(),
          events.impl.get(),
          &db->getRuntime().getMappings(),
          nullptr
        );
        builder = GameBuilder::create(*db);
      }

      static std::unique_ptr<IDatabase> createDB() {
        RuntimeDatabaseArgs args = DBReflect::createArgsWithMappings();
        DBReflect::addDatabase<TestDBT>(args);
        return std::make_unique<RuntimeDatabase>(std::move(args));
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
      const TableID from = db.builder->queryTables<Row<float>>().matchingTableIDs[0];
      const TableID to = db.builder->queryTables<Row<int64_t>>().matchingTableIDs[0];
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

          RuntimeTable::migrate(0, *tableFrom, *tableTo, 1);

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
    template<class T>
    static RuntimeDatabase createDatabase() {
      RuntimeDatabaseArgs args = DBReflect::createArgsWithMappings();
      DBReflect::addDatabase<T>(args);
      return RuntimeDatabase{ std::move(args) };
    }

    TEST_METHOD(Table_AddElement_SizeIncreases) {
      RuntimeDatabase db = createDatabase<Database<Table<Row<int>>>>();
      RuntimeTable& table = db[0];

      table.addElements(1);

      Assert::AreEqual(size_t(1), table.size());
    }

    TEST_METHOD(TableWithElement_getElement_HasValue) {
      RuntimeDatabase db = createDatabase<Database<Table<Row<int>>>>();
      RuntimeTable& table = db[0];
      const size_t i = table.addElements(1);
      auto* row = table.tryGet<Row<int>>();

      row->at(i) = 6;

      Assert::AreEqual(6, row->at(i));
    }

    struct IntRowA : Row<int> {};
    struct IntRowB : Row<int> {};

    TEST_METHOD(TwoRowsSameElementTypeDifferentValues_getRow_HasDifferentValues) {
      RuntimeDatabase db = createDatabase<Database<Table<IntRowA, IntRowB>>>();
      RuntimeTable& table = db[0];
      const size_t i = table.addElements(1);
      IntRowA* a = table.tryGet<IntRowA>();
      IntRowB* b = table.tryGet<IntRowB>();
      a->at(i) = 1;
      b->at(i) = 2;

      int rowA = a->at(i);
      int rowB = b->at(i);

      Assert::AreEqual(1, rowA);
      Assert::AreEqual(2, rowB);
    }

    TEST_METHOD(TableWithElement_swapRemove_IsRemoved) {
      RuntimeDatabase db = createDatabase<Database<Table<Row<int>>>>();
      RuntimeTable& table = db[0];
      table.addElements(1);

      table.swapRemove(0);

      Assert::AreEqual(size_t(0), table.size());
    }

    TEST_METHOD(TableWithRow_hasRow_True) {
      RuntimeDatabase db = createDatabase<Database<Table<Row<int>>>>();

      Assert::IsNotNull(db[0].tryGet<Row<int>>());
    }

    TEST_METHOD(TableWithoutRow_hasRow_False) {
      RuntimeDatabase db = createDatabase<Database<Table<Row<int>>>>();

      Assert::IsNull(db[0].tryGet<Row<std::string>>());
    }

    TEST_METHOD(SameTables_migrateOne_ValuesMoved) {
      using TabT = Table<Row<int>>;
      RuntimeDatabase db = createDatabase<Database<TabT, TabT>>();
      RuntimeTable& a = db[0];
      RuntimeTable& b = db[1];
      a.addElements(1);
      a.tryGet<Row<int>>()->at(0) = 5;

      RuntimeTable::migrate(0, a, b, 1);

      Assert::AreEqual(5, b.tryGet<Row<int>>()->at(0));
    }

    TEST_METHOD(SameTables_migrateAll_ValuesMoved) {
      using TabT = Table<Row<int>>;
      RuntimeDatabase db = createDatabase<Database<TabT, TabT>>();
      RuntimeTable& a = db[0];
      RuntimeTable& b = db[1];
      a.addElements(5);
      for(int i = 0; i < 5; ++i) {
        a.tryGet<Row<int>>()->at(i) = i;
      }

      RuntimeTable::migrate(0, a, b, 5);

      for(int i = 0; i < 5; ++i) {
        Assert::AreEqual(i, b.tryGet<Row<int>>()->at(i));
      }
    }

    TEST_METHOD(SmallAndBigTable_MigrateOneSmallToBig_ValuesMoved) {
      RuntimeDatabase db = createDatabase<Database<Table<Row<int>>, Table<Row<int>, Row<short>>>>();
      RuntimeTable& a = db[0];
      RuntimeTable& b = db[1];
      a.addElements(1);
      a.tryGet<Row<int>>()->at(0) = 5;

      RuntimeTable::migrate(0, a, b, 1);

      Assert::AreEqual(5, b.tryGet<Row<int>>()->at(0));
      Assert::AreEqual(size_t(1), b.size());
      Assert::AreEqual(short(0), b.tryGet<Row<short>>()->at(0));
    }

    TEST_METHOD(SmallAndBigTable_MigrateAllSmallToBig_ValuesMoved) {
      RuntimeDatabase db = createDatabase<Database<Table<Row<int>>, Table<Row<int>, Row<short>>>>();
      RuntimeTable& a = db[0];
      RuntimeTable& b = db[1];
      a.addElements(5);
      for(int i = 0; i < 5; ++i) {
        a.tryGet<Row<int>>()->at(i) = i;
      }

      RuntimeTable::migrate(0, a, b, 5);

      for(int i = 0; i < 5; ++i) {
        Assert::AreEqual(i, b.tryGet<Row<int>>()->at(i));
        Assert::AreEqual(short(0), b.tryGet<Row<short>>()->at(i));
      }
      Assert::AreEqual(size_t(5), b.size());
    }

    TEST_METHOD(SmallAndBigTable_MigrateOneBigToSmall_ValuesMoved) {
      RuntimeDatabase db = createDatabase<Database<Table<Row<int>>, Table<Row<int>, Row<short>>>>();
      RuntimeTable& a = db[0];
      RuntimeTable& b = db[1];
      b.addElements(1);
      b.tryGet<Row<int>>()->at(0) = 5;

      RuntimeTable::migrate(0, b, a, 1);

      Assert::AreEqual(5, a.tryGet<Row<int>>()->at(0));
    }

    TEST_METHOD(SmallAndBigTable_MigrateAllBigToSmall_ValuesMoved) {
      RuntimeDatabase db = createDatabase<Database<Table<Row<int>>, Table<Row<int>, Row<short>>>>();
      RuntimeTable& a = db[0];
      RuntimeTable& b = db[1];
      b.addElements(5);
      for(int i = 0; i < 5; ++i) {
        b.tryGet<Row<int>>()->at(i) = i;
      }

      RuntimeTable::migrate(0, b, a, 5);

      for(int i = 0; i < 5; ++i) {
        Assert::AreEqual(i, a.tryGet<Row<int>>()->at(i));
      }
    }

    TEST_METHOD(Tables_Query_AreFound) {
      RuntimeDatabase db = createDatabase<Database<
        Table<Row<int>>,
        Table<Row<int>, Row<short>>,
        Table<Row<uint64_t>>
      >>();
      db[0].addElements(1);
      db[0].tryGet<Row<int>>()->at(0) = 1;
      db[1].addElements(1);
      db[1].tryGet<Row<int>>()->at(0) = 2;
      db[2].addElements(1);
      db[2].tryGet<Row<uint64_t>>()->at(0) = uint64_t(4);

      int total = 0;
      auto q = db.query<Row<int>>();
      q.forEachElement([&](int& i) { total += i; });

      Assert::AreEqual(3, total);
    }

    TEST_METHOD(TableWithSharedRow_GetValue_IsSameForAll) {
      RuntimeDatabase db = createDatabase<Database<Table<Row<int>, SharedRow<int>>>>();
      RuntimeTable& table = db[0];
      table.tryGet<SharedRow<int>>()->at() = 5;
      table.addElements(2);

      const int a = table.tryGet<SharedRow<int>>()->at(0);
      const int b = table.tryGet<SharedRow<int>>()->at(1);

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

    using TestDB = Database<Table<Row<int>>, Table<Row<size_t>>>;
    static_assert(TestDB::ElementID(1, 0) == TestDB::getTableIndex<Table<Row<size_t>>>());

    struct SpatialPairsData {
      SpatialPairsData(RuntimeDatabaseTaskBuilder& task) {
        SP::IslandGraphRow* g{};
        auto q = task.query<SP::IslandGraphRow, SP::ObjA, SP::ObjB, SP::PairTypeRow, SP::ManifoldRow>();
        table = q.matchingTableIDs[0];
        std::tie(g, objA, objB, pairType, manifold) = q.get(0);
        graph = &g->at();
        ids = task.getIDResolver();
      }

      //Returns the index of the pair in the spatial pairs storage table if this pair has an entry
      std::optional<size_t> tryFindPair(const ElementRef& a, const ElementRef& b) {
        //Swaps the order of the pair in the same way broadphase and SpatialPairsStorage is expecting
        const Broadphase::SweepCollisionPair pair{ a, b };
        if(auto it = graph->findEdge(pair.a, pair.b); it != graph->edgesEnd()) {
          return *it;
        }
        return {};
      }

      UnpackedDatabaseElementID table;
      IslandGraph::Graph* graph{};
      SP::ObjA* objA{};
      SP::ObjB* objB{};
      SP::PairTypeRow* pairType{};
      SP::ManifoldRow* manifold{};
      std::shared_ptr<IIDResolver> ids;
    };

    TEST_METHOD(CollidingPair_PopulateNarrowphase_IsPopulated) {
      TestGame game;
      GameArgs args;
      args.fragmentCount = 2;
      game.init(args);

      TransformAdapter transform = TableAdapters::getTransform(game.builder(), game.tables.fragments);
      transform.posX->at(0) = 1.1f;
      transform.posX->at(1) = 1.2f;

      game.update();

      auto [stable] = game.builder().query<StableIDRow>(game.tables.fragments).get(0);
      SpatialPairsData pairs{ game.builder() };
      auto index = pairs.tryFindPair(stable->at(0), stable->at(1));
      Assert::IsTrue(index.has_value());
      Assert::IsTrue(pairs.manifold->at(*index).size > 0);
    }

    TEST_METHOD(DistantPair_PopulateNarrowphase_NoPairs) {
      TestGame game;
      GameArgs args;
      args.fragmentCount = 2;
      game.init(args);

      auto transform = TableAdapters::getTransform(game.builder(), game.tables.fragments);
      transform.posX->at(0) = 1.0f;
      transform.posX->at(1) = 5.0f;

      game.update();

      auto [stable] = game.builder().query<StableIDRow>(game.tables.fragments).get(0);
      SpatialPairsData pairs{ game.builder() };
      auto index = pairs.tryFindPair(stable->at(0), stable->at(1));
      Assert::IsFalse(index.has_value());
    }

    TEST_METHOD(TwoPairsSameObject_GenerateCollisionPairs_HasPairs) {
      TestGame game;
      GameArgs args;
      args.fragmentCount = 3;
      game.init(args);
      auto& stable = game.builder().query<StableIDRow>(game.tables.fragments).get<0>(0);

      auto transform = TableAdapters::getTransform(game.builder(), game.tables.fragments);
      for(int i = 0; i < args.fragmentCount; ++i) {
        transform.posY->at(i) = 0;
      }
      auto& posX = *transform.posX;
      //This one to collide with both
      posX.at(0) = 5.0f;
      //This one to the left to collide with 1 but not 2
      posX.at(1) = 4.0f;
      //To the right, colliding with 0 but not 1
      posX.at(2) = 6.0f + SweepNPruneBroadphase::BoundariesConfig{}.mPadding;

      game.update();

      SpatialPairsData pairs{ game.builder() };

      const ElementRef a{ stable.at(0) };
      const ElementRef b{ stable.at(1) };
      const ElementRef c{ stable.at(2) };
      Assert::IsTrue(pairs.tryFindPair(a, b).has_value());
      Assert::IsTrue(pairs.tryFindPair(a, c).has_value());
    }

    TEST_METHOD(CollidingPair_GenerateContacts_AreGenerated) {
      TestGame game;
      GameArgs args;
      args.fragmentCount = 2;
      game.init(args);
      TransformAdapter transform = TableAdapters::getTransform(game.builder(), game.tables.fragments);
      auto& posX = *transform.posX;
      const float expectedOverlap = 0.1f;
      posX.at(0) = 5.0f;
      posX.at(1) = 6.0f - expectedOverlap;
      transform.posY->at(0) = transform.posY->at(1) = 0.0f;

      game.update();

      SpatialPairsData pairs{ game.builder() };
      auto index = pairs.tryFindPair(game.getFromTable(game.tables.fragments, 0), game.getFromTable(game.tables.fragments, 1));
      Assert::IsTrue(index.has_value());
      const float e = 0.00001f;
      const auto& man = pairs.manifold->at(*index);
      Assert::AreEqual(expectedOverlap, man[0].overlap, e);
      Assert::AreEqual(0.5f - expectedOverlap, man[0].centerToContactA.x, e);
      Assert::AreEqual(-1.0f, man[0].normal.x, e);
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

    static void verifyAllMappings(RuntimeDatabase& db) {
      for(size_t t = 0; t < db.size(); ++t) {
        RuntimeTable& table = db[t];
        StableIDRow* stable = table.tryGet<StableIDRow>();
        if(!stable) {
          continue;
        }
        for(size_t i = 0; i < table.size(); ++i) {
          ElementRef ref = stable->at(i);
          const StableElementMappingPtr m = ref.getMapping();
          Assert::IsTrue(static_cast<bool>(m));
          const UnpackedDatabaseElementID expectedID = table.getID().remakeElement(i);
          const UnpackedDatabaseElementID actual{ m->unstableIndex, expectedID.mElementIndexBits };
          Assert::IsTrue(expectedID == actual);
        }
      }
    }

    TEST_METHOD(StableElementID_Operations) {
      RuntimeDatabase db = createDatabase<TestStableDB>();
      auto& a = db[1];
      auto& b = db[2];
      auto& stableA = *a.tryGet<StableIDRow>();
      auto& valueA = *a.tryGet<Row<int>>();
      constexpr auto tableIndexA = UnpackedDatabaseElementID::fromPacked(TestStableDB::getTableIndex<StableTableA>());
      constexpr auto tableIndexB = UnpackedDatabaseElementID::fromPacked(TestStableDB::getTableIndex<StableTableB>());
      a.resize(3);
      ElementRefResolver res{ db.getDescription() };

      verifyAllMappings(db);

      valueA.at(0) = 1;
      valueA.at(2) = 5;
      ElementRef elementA = stableA.at(tableIndexA.getElementIndex());
      ElementRef elementC = stableA.at(2);
      a.swapRemove(0);

      verifyAllMappings(db);

      Assert::AreEqual(5, valueA.at(0));
      Assert::IsFalse(static_cast<bool>(elementA));
      TestStableDB::ElementID resolvedC{ res.uncheckedUnpack(elementC).mValue };
      Assert::AreEqual(5, db[resolvedC.getTableIndex()].tryGet<Row<int>>()->at(resolvedC.getElementIndex()));

      //Migrate object at index 0 in A which is ElementC
      RuntimeTable::migrate(0, a, b, 1);

      verifyAllMappings(db);

      resolvedC = TestStableDB::ElementID{ res.uncheckedUnpack(elementC).mValue };
      Assert::AreEqual(5, db[resolvedC.getTableIndex()].tryGet<Row<int>>()->at(resolvedC.getElementIndex()));

      a.resize(0);
      b.resize(0);

      Assert::IsTrue(db.getMappings().empty());
    }

    static void assertUnorderedCollisionPairsMatch(TestGame& game, std::vector<ElementRef> expectedA, std::vector<ElementRef> expectedB) {
      struct Contact {
        ElementRef a, b;
      };
      SpatialPairsData pairs{ game.builder() };
      std::vector<Contact> contacts;
      contacts.reserve(expectedA.size());
      for(size_t i = 0; i < pairs.objA->size(); ++i) {
        if(pairs.pairType->at(i) == SP::PairType::ContactXY && pairs.objA->at(i)) {
          contacts.push_back({ .a{ pairs.objA->at(i) }, .b{ pairs.objB->at(i) } });
        }
      }

      Assert::AreEqual(expectedA.size(), contacts.size());
      //TODO: not validating anything about contacts. Should it?
      for(size_t i = 0; i < contacts.size(); ++i) {
        Assert::IsTrue(pairs.tryFindPair(expectedA[i], expectedB[i]).has_value());
      }
    }

    static void assertEnabledContactConstraintCount(TestGame& game, size_t expected) {
      SpatialPairsData pairs{ game.builder() };
      auto resolver = ConstraintSolver::createResolver(game.builder(), PhysicsSimulation::getPhysicsAliases());
      size_t actualCount{};
      auto ref = pairs.ids->getRefResolver();
      for(size_t i = 0; i < pairs.manifold->size(); ++i) {
        //If there are no contacts to solve, this one doesn't count
        if(!pairs.manifold->at(i).size) {
          continue;
        }
        auto ra = ref.tryUnpack(pairs.objA->at(i));
        auto rb = ref.tryUnpack(pairs.objB->at(i));
        if(ra && rb) {
          ConstraintSolver::ConstraintBody ba, bb;
          ba = resolver->resolve(*ra);
          bb = resolver->resolve(*rb);
          //Only count constraints between dynamic pairs that pass the mask
          if(ba.mass && bb.mass && (ba.constraintMask & bb.constraintMask)) {
            ++actualCount;
          }
        }
      }
      Assert::AreEqual(expected, actualCount);
    }

    static void assertEnabledStaticContactConstraintCount(TestGame& game, size_t expected) {
      SpatialPairsData pairs{ game.builder() };
      auto resolver = ConstraintSolver::createResolver(game.builder(), PhysicsSimulation::getPhysicsAliases());
      size_t actualCount{};
      auto ref = pairs.ids->getRefResolver();
      for(size_t i = 0; i < pairs.manifold->size(); ++i) {
        //If there are no contacts to solve, this one doesn't count
        if(!pairs.manifold->at(i).size) {
          continue;
        }
        auto ra = ref.tryUnpack(pairs.objA->at(i));
        auto rb = ref.tryUnpack(pairs.objB->at(i));
        if(ra && rb) {
          ConstraintSolver::ConstraintBody ba, bb;
          ba = resolver->resolve(*ra);
          bb = resolver->resolve(*rb);
          //Only count constraints between dynamic to static pairs that pass the mask
          if((!ba.mass || !bb.mass) && (ba.constraintMask & bb.constraintMask)) {
            ++actualCount;
          }
        }
      }
      Assert::AreEqual(expected, actualCount);
    }

    TEST_METHOD(GameOneObject_Migrate_PhysicsDataPreserved) {
      TestGame game;
      GameArgs gameArgs;
      gameArgs.fragmentCount = 1;
      gameArgs.playerPos = glm::vec2(5, 5);
      game.init(gameArgs);
      ElementRef playerId = game.getFromTable(game.tables.player, 0);
      ElementRef objectId = game.getFromTable(game.tables.fragments, 0);
      auto res = game.builder().getIDResolver()->getRefResolver();
      const float minCorrection = 0.1f;
      TransformAdapter playerTransform = TableAdapters::getTransform(game.builder(), game.tables.player);
      TransformAdapter fragmentTransform = TableAdapters::getTransform(game.builder(), game.tables.fragments);
      TransformAdapter completedFragmentTransform = TableAdapters::getTransform(game.builder(), game.tables.completedFragments);
      PhysicsObjectAdapter playerPhysics = TableAdapters::getPhysics(game.builder(), game.tables.player);
      PhysicsObjectAdapter fragmentPhysics = TableAdapters::getPhysics(game.builder(), game.tables.fragments);
      SpatialPairsData pairs{ game.builder() };

      auto setInitialPos = [&] {
        TableAdapters::write(0, { 1.5f, 0.0f }, *playerTransform.posX, *playerTransform.posY);
        TableAdapters::write(0, { 1.0f, 0.0f }, *fragmentTransform.posX, *fragmentTransform.posY);
        playerPhysics.linVelX->at(0) = -0.5f;
        fragmentPhysics.linVelX->at(0) = 0.5f;
      };
      setInitialPos();
      game.update();

      auto assertInitialResolution = [&] {
        assertEnabledContactConstraintCount(game, 1);
        assertEnabledStaticContactConstraintCount(game, 0);
        Assert::IsTrue(pairs.tryFindPair(playerId, objectId).has_value());
        Assert::IsTrue(playerPhysics.linVelX->at(0) > -0.5f + minCorrection, L"Player should be pushed away from object");
        Assert::IsTrue(fragmentPhysics.linVelX->at(0) < 0.5f - minCorrection, L"Object should be pushed away from player");
      };
      assertInitialResolution();

      playerTransform.posX->at(0) = 100.0f;
      game.update();

      auto assertNoPairs = [&] {
        assertEnabledContactConstraintCount(game, 0);
        assertEnabledStaticContactConstraintCount(game, 0);
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
      auto unpacked = res.uncheckedUnpack(objectId);
      Assert::AreEqual(*objectId.tryGet(), game.tables.completedFragments.remakeElement(0).mValue);

      game.update();
      auto assertStaticCollision = [&] {
        //Similar to before, except now the single constraint is in the static table instead of the dynamic one
        //Pair order is the same, both because player has lower stable id (0) than object (1) but also because object is now static,
        //and static objects always order B in pairs
        assertEnabledContactConstraintCount(game, 0);
        assertEnabledStaticContactConstraintCount(game, 1);
        Assert::IsTrue(pairs.tryFindPair(playerId, objectId).has_value());

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
      const size_t result = grid.cells[cell].axis[0].elements.size() / 2;
      Assert::AreEqual(result, grid.cells[cell].containedKeys.size());
      Assert::AreEqual(result, grid.cells[cell].axis[1].elements.size() / 2);
      return result;
    }

    size_t trackedCellPairs(const Broadphase::SweepGrid::Grid& grid, size_t cellIndex) {
      const Broadphase::Sweep2D& cell = grid.cells[cellIndex];
      return std::count_if(grid.pairs.trackedPairs.begin(), grid.pairs.trackedPairs.end(), [&](const auto& pair) {
        return cell.containedKeys.count(Broadphase::BroadphaseKey{ pair.a }) &&
          cell.containedKeys.count(Broadphase::BroadphaseKey{ pair.b });
      });
    }

    TEST_METHOD(BroadphaseBoundaries) {
      GameConstructArgs gca;
      Config::PhysicsConfig& cfg = gca.physics;
      cfg.broadphase.bottomLeftX = 0.0f;
      cfg.broadphase.bottomLeftY = 0.0f;
      cfg.broadphase.cellCountX = 2;
      cfg.broadphase.cellCountY = 1;
      cfg.broadphase.cellSizeX = 10.0f;
      cfg.broadphase.cellSizeY = 10.0f;
      //Use negative padding to make the cell size 0.5 instead of 0.7 so overlapping cells are also colliding
      cfg.broadphase.cellPadding = 0.5f - SweepNPruneBroadphase::BoundariesConfig::UNIT_CUBE_EXTENTS;
      gca.updateConfig.enableFragmentStateMachine = false;
      TestGame game{ std::move(gca) };
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

        assertEnabledContactConstraintCount(game, 3);
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
      assertEnabledContactConstraintCount(game, 2);

      //Move boundary object to the right cell
      objs.posX->at(1) = cfg.broadphase.cellSizeX + halfSize*2;
      objs.posX->at(0) = cfg.broadphase.cellSizeX - halfSize - padding;
      objs.posX->at(2) = cfg.broadphase.cellSizeX + halfSize + padding;

      game.update();

      Assert::AreEqual(size_t(1), cellSize(grid, 0));
      Assert::AreEqual(size_t(2), cellSize(grid, 1));
      Assert::AreEqual(size_t(0), trackedCellPairs(grid, 0));
      Assert::AreEqual(size_t(1), trackedCellPairs(grid, 1));
      assertEnabledContactConstraintCount(game, 1);

      //Move boundary object to the left cell
      objs.posX->at(1) = cfg.broadphase.cellSizeX - halfSize*2;
      objs.posX->at(0) = cfg.broadphase.cellSizeX - halfSize - padding;
      objs.posX->at(2) = cfg.broadphase.cellSizeX + halfSize + padding;

      game.update();

      Assert::AreEqual(size_t(2), cellSize(grid, 0));
      Assert::AreEqual(size_t(1), cellSize(grid, 1));
      Assert::AreEqual(size_t(1), trackedCellPairs(grid, 0));
      Assert::AreEqual(size_t(0), trackedCellPairs(grid, 1));
      assertEnabledContactConstraintCount(game, 1);

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
      assertEnabledContactConstraintCount(game, 1);

      setBoundary();
      objs.posX->at(0) = cfg.broadphase.cellSizeX + halfSize + padding;

      game.update();

      Assert::AreEqual(size_t(1), cellSize(grid, 0));
      Assert::AreEqual(size_t(3), cellSize(grid, 1));
      Assert::AreEqual(size_t(0), trackedCellPairs(grid, 0));
      Assert::AreEqual(size_t(1), trackedCellPairs(grid, 1));
      assertEnabledContactConstraintCount(game, 1);
    }

    TEST_METHOD(GameTwoObjects_Migrate_PhysicsDataPreserved) {
      GameArgs gameArgs;
      gameArgs.fragmentCount = 2;
      gameArgs.playerPos = glm::vec2{ 5, 5 };
      GameConstructArgs gca;
      //Fragment state machine will create spatial query objects that throw off the expected counts
      gca.updateConfig.enableFragmentStateMachine = false;
      TestGame game{ std::move(gca) };
      game.init(gameArgs);
      auto task = game.builder();
      auto ids = task.getIDResolver();
      TransformAdapter fragmentTransform = TableAdapters::getTransform(task, game.tables.fragments);
      PhysicsObjectAdapter fragmentPhysics = TableAdapters::getPhysics(task, game.tables.fragments);
      TransformAdapter playerTransform = TableAdapters::getTransform(task, game.tables.player);
      PhysicsObjectAdapter playerPhysics = TableAdapters::getPhysics(task, game.tables.player);

      StableIDRow& stablePlayer = task.query<StableIDRow>(game.tables.player).get<0>(0);
      StableIDRow& stableFragment = task.query<StableIDRow>(game.tables.fragments).get<0>(0);
      ElementRef playerId = stablePlayer.at(0);
      ElementRef objectLeftId = stableFragment.at(0);
      ElementRef objectRightId = stableFragment.at(1);

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

      SpatialPairsData pairs{ game.builder() };
      assertEnabledContactConstraintCount(game, 3);
      assertEnabledStaticContactConstraintCount(game, 0);
      assertUnorderedCollisionPairsMatch(game,
        { playerId, playerId, objectLeftId },
        { objectLeftId, objectRightId, objectRightId }
      );

      //Min ia a bit weirder here since the impulse is spread between the two objects and at an angle
      const float minCorrection = 0.05f;
      Assert::IsTrue(playerPhysics.linVelY->at(0) > -0.5f + minCorrection, L"Player should be pushed away from object");
      Assert::IsTrue(fragmentPhysics.linVelY->at(0) < 0.5f - minCorrection, L"Object should be pushed away from player");
      Assert::IsTrue(fragmentPhysics.linVelY->at(1) < 0.5f - minCorrection, L"Object should be pushed away from player");

      auto&& [goalFound, goalX, goalY] = task.query<FragmentGoalFoundRow, FloatRow<Tags::FragmentGoal, Tags::X>, FloatRow<Tags::FragmentGoal, Tags::Y>>().get(0);
      goalFound->at(0) = true;
      TableAdapters::write(0, glm::vec2{ initialX[0], initialY[0] }, *goalX, *goalY);
      game.update();

      auto resetStaticPos = [&] {
        setInitialPlayerPos();
        fragmentTransform.posX->at(0) = initialRight.x;
        fragmentTransform.posY->at(0) = initialRight.y + 0.1f;
        fragmentPhysics.linVelY->at(0) = 0.5f;
      };

      resetStaticPos();
      game.update();

      auto assertStaticCollision = [&] {
        assertEnabledContactConstraintCount(game, 1);
        assertEnabledStaticContactConstraintCount(game, 2);
        //Now left is the B object always since it's static
        assertUnorderedCollisionPairsMatch(game,
          { playerId, playerId, objectRightId },
          { objectLeftId, objectRightId, objectLeftId }
        );

        Assert::IsTrue(playerPhysics.linVelY->at(0) > -0.5f + minCorrection, L"Player should be pushed away from object");
        Assert::IsTrue(fragmentPhysics.linVelY->at(0) < 0.5f - minCorrection, L"Object should be pushed away from player");
      };
      assertStaticCollision();

      resetStaticPos();
      playerTransform.posX->at(0) = 100.0f;
      game.update();

      assertEnabledContactConstraintCount(game, 0);
      assertEnabledStaticContactConstraintCount(game, 1);
      assertUnorderedCollisionPairsMatch(game,
        { objectRightId },
        { objectLeftId }
      );

      resetStaticPos();
      game.update();

      assertStaticCollision();
    }

    TEST_METHOD(UnpackedDBElementID) {
      using SampleDB = Database<
        Table<Row<int>>,
        Table<Row<char>>,
        Table<SharedRow<uint32_t>>
      >;

      auto id = SampleDB::getElementID<Table<Row<char>>>(5);
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

        const ElementRef idA = query.get<0>(0).at(0);
        const ElementRef idB = query.get<0>(0).at(1);
        const ElementRef idC = query.get<0>(0).at(2);

        {
          LambdaStatEffect::Builder lambda{ args };
          lambda.createStatEffects(1).setLifetime(StatEffect::INSTANT).setOwner(idA);
          lambda.setLambda([&test, ids](LambdaStatEffect::Args& args) {
            Assert::AreEqual(size_t(0), ids->getRefResolver().uncheckedUnpack(args.resolvedID).getElementIndex());
            ++test.lambdaInvocations;
          });
        }
        {
          VelocityStatEffect::Builder vel{ args };
          vel.createStatEffects(1).setLifetime(1).setOwner(idB);
          VelocityStatEffect::ImpulseCommand icmd;
          icmd.linearImpulse = glm::vec2(1.0f);
          icmd.angularImpulse = 1.0f;
          vel.addImpulse(icmd);
        }
        {
          PositionStatEffect::Builder pos{ args };
          pos.createStatEffects(1).setOwner(idC).setLifetime(0);
          pos.setPos(glm::vec2(5.0f));
          pos.setRot(glm::vec2(0.0f, 1.0f));
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
        fragmentTransform.posY->at(i) = 0;
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
      Assert::AreEqual(size_t(0), velLife->at(0));

      Assert::AreEqual(5.0f, fragmentTransform.posX->at(2));
      Assert::AreEqual(5.0f, fragmentTransform.posY->at(2));
      Assert::AreEqual(0.0f, fragmentTransform.rotX->at(2));
      Assert::AreEqual(1.0f, fragmentTransform.rotY->at(2));
      Assert::AreEqual(size_t(0), posLife->size());

      //Set the values to something to show they don't change after the next update
      fragmentPhysics.linVelX->at(1) = 0.0f;
      fragmentTransform.posX->at(2) = 10.0f;

      //After this pos and lambda should be removed and should not have executed again before removal
      game.update();

      Assert::AreEqual(1, test.lambdaInvocations);
      Assert::AreEqual(size_t(0), lambdaLife->size());
      Assert::AreEqual(size_t(0), posLife->size());
      Assert::AreEqual(size_t(0), velLife->size());
      //Assert the unchanged values
      Assert::AreEqual(1.0f, fragmentPhysics.linVelX->at(1), 0.1f);
      Assert::AreEqual(10.0f, fragmentTransform.posX->at(2), 0.1f);

      fragmentPhysics.linVelX->at(1) = 0.0f;

      //Should remove velcity
      game.update();

      Assert::AreEqual(0.0f, fragmentPhysics.linVelX->at(1), 0.1f);
    }

    TEST_METHOD(PlayerInputTest) {
      GameArgs args;
      args.playerPos = glm::vec2{ 0.0f };
      TestGame game{ args };
      auto [playerInput, stateMachine, posX] = game.builder().query<GameInput::PlayerInputRow, GameInput::StateMachineRow, FloatRow<Tags::Pos, Tags::X>>().get(0);
      const Input::InputMapper& mapper = stateMachine->at(0).getMapper();
      stateMachine->at(0).traverse(mapper.onPassthroughAxis2DAbsolute(GameInput::Keys::MOVE_2D, { 1, 0 }));

      //Once to compute the impulse, next frame updates position with it
      game.update();
      game.update();

      Assert::IsTrue(posX->at(0) > 0.0f);
    }

    TEST_METHOD(GameplayExtract) {
      GameArgs args;
      args.fragmentCount = 1;
      args.completedFragmentCount = 1;
      args.playerPos = glm::vec2{ 0.0f };
      TestGame game{ args };

      GameObjectAdapter fragment = TableAdapters::getGameObject(game.builder(), game.tables.fragments);
      GameObjectAdapter completedFragment = TableAdapters::getGameObject(game.builder(), game.tables.completedFragments);
      GameObjectAdapter player = TableAdapters::getGameObject(game.builder(), game.tables.player);
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

      setValues(fragment, 1.0f);
      setValues(completedFragment, 2.0f);
      setValues(player, 3.0f);

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

      assertValues(TableAdapters::getGameplayGameObject(game.builder(), game.tables.fragments), 1.0f);
      assertValues(TableAdapters::getGameplayGameObject(game.builder(), game.tables.completedFragments), 2.0f);
      assertValues(TableAdapters::getGameplayGameObject(game.builder(), game.tables.player), 3.0f);
    }

    static auto addGlobalPointForce(TestStatInfo& test) {
      return [&test](IAppBuilder& builder) {
        auto task = builder.createTask();
        task.setName("test");
        task.setCallback([&test](AppTaskArgs& args) {
          if(test.shouldRun) {
            AreaForceStatEffect::Builder effect{ args };
            effect.createStatEffects(1).setLifetime(StatEffect::INSTANT);
            effect.setOrigin(glm::vec2{ 4.0f, 0.0f });
            effect.setDirection(glm::vec2{ 1.0f, 0.0f });
            effect.setPiercing(3.0f, 0.0f);
            effect.setRayCount(4);
            AreaForceStatEffect::Command::Cone cone;
            cone.halfAngle = 0.25f;
            cone.length = 15.0f;
            effect.setShape(cone);
            effect.setImpulse(AreaForceStatEffect::Command::FlatImpulse{ 1.0f });
          }
        });
        builder.submitTask(std::move(task));
      };
    }

    TEST_METHOD(GlobalPointForce) {
      GameArgs args;
      args.fragmentCount = 1;
      GameConstructArgs construct;
      TestStatInfo test;
      construct.updateConfig.injectGameplayTasks = addGlobalPointForce(test);
      TestGame game{ std::move(construct) };
      game.init(args);

      GameObjectAdapter fragment = TableAdapters::getGameObject(game.builder(), game.tables.fragments);
      fragment.transform.posX->at(0) = 5.0f;
      fragment.transform.rotX->at(0) = 1.0f;

      //One update to request the impulse, then the next to apply it
      test.shouldRun = true;
      game.update();
      test.shouldRun = false;
      game.update();

      auto&& [cmd] = game.builder().query<AreaForceStatEffect::CommandRow>().get(0);
      Assert::IsTrue(fragment.physics.linVelX->at(0) > 0.0f);
      Assert::AreEqual(size_t(0), cmd->size());
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

    void assertQueryHas(TestGame& game, const ElementRef& query, std::vector<ElementRef> expected) {
      auto q = SpatialQuery::createReader(game.builder());
      q->begin(query);
      const SpatialQuery::Result* r = q->tryIterate();
      auto ids = game.builder().getIDResolver();

      size_t count = 0;
      while(r && expected.size() > count) {
        Assert::IsTrue(expected[count] == r->other);

        ++count;
        r = q->tryIterate();
      }
      Assert::AreEqual(expected.size(), count);
    }

    void assertQueryHasObject(TestGame& game, const ElementRef& query, const ElementRef& object) {
      assertQueryHas(game, query, { object });
    }

    void assertQueryNoObjects(TestGame& game, const ElementRef& query) {
      assertQueryHas(game, query, {});
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

      auto objs = TableAdapters::getGameObject(game.builder(), game.tables.fragments);
      const ElementRef objID = objs.stable->at(0);
      auto creator = SpatialQuery::createCreator(game.builder());
      ElementRef bb = creator->createQuery({ SpatialQuery::AABB{ glm::vec2(2.0f), glm::vec2(3.5f) } }, 10);
      ElementRef circle = creator->createQuery({ SpatialQuery::Circle{ glm::vec2(0, 5), 1.5f } }, 10);
      ElementRef cast = creator->createQuery({ SpatialQuery::Raycast{ glm::vec2(-1.0f), glm::vec2(2.0f, -1.0f) } }, 10);
      //Update to create the queries
      game.update();

      //Move object into aabb
      TableAdapters::write(0, glm::vec2(2.5f), *objs.transform.posX, *objs.transform.posY);
      game.update();

      assertQueryHasObject(game, bb, objID);
      assertQueryNoObjects(game, circle);
      assertQueryNoObjects(game, cast);

      //Move aabb out of object and move circle in
      auto writer = SpatialQuery::createWriter(game.builder());
      writer->refreshQuery(bb, { SpatialQuery::AABB{ glm::vec2(20), glm::vec2(21) } }, 10);
      writer->refreshQuery(circle, { SpatialQuery::Circle{ glm::vec2(2), 1.0f } }, 10);
      tickQueryUpdate(game);

      assertQueryNoObjects(game, bb);
      assertQueryHasObject(game, circle, objID);
      assertQueryNoObjects(game, cast);

      //Move object to raycast and circle with it
      TableAdapters::write(0, glm::vec2(2.f, -1.f), *objs.transform.posX, *objs.transform.posY);
      writer->refreshQuery(circle, { SpatialQuery::Circle{ glm::vec2(2, -1), 0.5f } }, 10);
      tickQueryUpdate(game);

      assertQueryNoObjects(game, bb);
      assertQueryHasObject(game, circle, objID);
      assertQueryHasObject(game, cast, objID);

      //Move circle out and turn raycast into an aabb
      writer->refreshQuery(circle, { SpatialQuery::Circle{ glm::vec2(20), 1.0f } }, 10);
      //Shape changing not supported anymore
      //writer->refreshQuery(cast, { SpatialQuery::AABB{ glm::vec2(2.f, -1.f), glm::vec2(2.5f, 0.f) } }, 10);
      tickQueryUpdate(game);

      assertQueryNoObjects(game, bb);
      assertQueryNoObjects(game, circle);
      //assertQueryHasObject(game, cast, objID);

      //Try a query with a single tick lifetime
      ElementRef single = creator->createQuery({ SpatialQuery::Circle{ glm::vec2(2, -1), 1.f } }, SpatialQuery::SINGLE_USE);
      game.update();
      assertQueryNoObjects(game, single);
      auto taskArgs = game.sharedArgs();
      LambdaStatEffect::Builder lambda{ taskArgs };
      //auto lambda = TableAdapters::getLambdaEffects(taskArgs);
      //Query should be resolved during physics then viewable later by gameplay but destroyed at the end of the frame.
      //Catch it in the middle with a lambda stat effect which should execute before the removal but after it's resolved
      lambda.createStatEffects(1).setLifetime(StatEffect::INSTANT).setOwner(objID);
      lambda.setLambda([&](...) {
        assertQueryHasObject(game, single, objID);
      });
      game.update();
      auto reader = SpatialQuery::createReader(game.builder());
      reader->begin(single);
      Assert::IsNull(reader->tryIterate());
    }

    TEST_METHOD(CollisionMasks) {
      GameArgs args;
      args.fragmentCount = 2;
      TestGame game{ args };
      GameObjectAdapter objs = TableAdapters::getGameObject(game.builder(), game.tables.fragments);

      objs.transform.posX->at(0) = 1;
      objs.transform.posX->at(1) = 1.1f;
      objs.transform.posY->at(0) = objs.transform.posY->at(1) = 0.0f;
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

    /* TODO
    TEST_METHOD(FragmentStateMachine) {
      GameArgs args;
      args.fragmentCount = 2;
      TestGame game{ args };
      auto [state, stable, goalFound] = game.builder().query<
        FragmentStateMachine::StateRow,
        StableIDRow,
        FragmentGoalFoundRow
      >(game.tables.fragments).get(0);

      FragmentStateMachine::FragmentState::Variant desiredState{ FragmentStateMachine::SeekHome{} };
      FragmentStateMachine::setState(state->at(0), FragmentStateMachine::FragmentState::Variant{ desiredState });

      game.update();

      Assert::AreEqual(desiredState.index(), state->at(0).currentState.index());
      StableElementID target = std::get<FragmentStateMachine::SeekHome>(state->at(0).currentState).target;

      goalFound->at(0) = true;
      game.update();
      game.update();

      auto ids = game.builder().getIDResolver();
      Assert::IsFalse(ids->tryResolveStableID(target).has_value(), L"Target created by state should have been destroyed when object was migrated to a table without state");

      //Do the same state transition for the second object
      FragmentStateMachine::setState(state->at(0), FragmentStateMachine::FragmentState::Variant{ desiredState });

      game.update();

      Assert::AreEqual(desiredState.index(), state->at(0).currentState.index());
      target = std::get<FragmentStateMachine::SeekHome>(state->at(0).currentState).target;

      //Enqueue removal of the object. The destruction should still remove the target
      auto taskArgs = game.sharedArgs();
      Events::onRemovedElement(StableElementID::fromStableRow(0, *stable), taskArgs);

      game.update();
      game.update();

      Assert::IsFalse(ids->tryResolveStableID(target).has_value(), L"Target created by state should have been destroyed before object was destroyed");
    }
    */
  };
}