#include "Precompile.h"
#include "CppUnitTest.h"

#include "Physics.h"
#include "Simulation.h"
#include "Table.h"
#include "TableOperations.h"
#include "Queries.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Test {
  static_assert(std::is_same_v<Duple<int*>, Table<Row<int>>::ElementRef>);
  static_assert(std::is_same_v<Table<Row<int>>::ElementRef, decltype(make_duple((int*)nullptr))>);
  static_assert(std::is_same_v<std::tuple<DupleElement<0, int>>, Duple<int>::TupleT>);

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

    static void _fillNarrowphaseData(GameDatabase& db) {
      auto& pairs = std::get<CollisionPairsTable>(db.mTables);
      Physics::fillNarrowphaseData<
        FloatRow<Tags::Pos, Tags::X>,
        FloatRow<Tags::Pos, Tags::Y>,
        FloatRow<Tags::Rot, Tags::CosAngle>,
        FloatRow<Tags::Rot, Tags::SinAngle>>(pairs, db);
    }

    static void _fillConstraintVelocities(GameDatabase& db) {
      auto& constraints = std::get<ConstraintsTable>(db.mTables);
      Physics::fillConstraintVelocities<
        FloatRow<Tags::LinVel, Tags::X>,
        FloatRow<Tags::LinVel, Tags::Y>,
        FloatRow<Tags::AngVel, Tags::Angle>>(constraints, db);
    }

    static void _storeConstraintVelocities(GameDatabase& db) {
      auto& constraints = std::get<ConstraintsTable>(db.mTables);
      Physics::storeConstraintVelocities<
        FloatRow<Tags::LinVel, Tags::X>,
        FloatRow<Tags::LinVel, Tags::Y>,
        FloatRow<Tags::AngVel, Tags::Angle>>(constraints, db);
    }

    TEST_METHOD(CollidingPair_PopulateNarrowphase_IsPopulated) {
      GameDatabase db;
      auto& gameobjects = std::get<GameObjectTable>(db.mTables);
      TableOperations::resizeTable(gameobjects, 2);
      auto& broadphase = std::get<BroadphaseTable>(db.mTables);
      auto& dimensions = std::get<SharedRow<GridBroadphase::RequestedDimensions>>(broadphase.mRows).at();
      dimensions.mMin.x = 0;
      dimensions.mMax.x = 10;
      dimensions.mMax.y = 9;
      dimensions.mMin.y = -1;
      auto& posX = std::get<FloatRow<Tags::Pos, Tags::X>>(gameobjects.mRows);
      posX.at(0) = 1.1f;
      posX.at(1) = 1.2f;
      auto& posY = std::get<FloatRow<Tags::Pos, Tags::Y>>(gameobjects.mRows);
      auto& pairs = std::get<CollisionPairsTable>(db.mTables);

      Physics::allocateBroadphase(broadphase);
      Physics::rebuildBroadphase(db.getTableIndex<GameObjectTable>().mValue, posX.mElements.data(), posY.mElements.data(), broadphase, size_t(2));
      Assert::IsTrue(std::get<SharedRow<GridBroadphase::Overflow>>(broadphase.mRows).at().mElements.empty());

      Physics::generateCollisionPairs(broadphase, pairs);
      _fillNarrowphaseData(db);

      Assert::AreEqual(size_t(1), TableOperations::size(pairs));
      auto& narrowphasePosX = std::get<NarrowphaseData<PairA>::PosX>(pairs.mRows);
      //Don't really care what order it copied in as long as the values were copied from gameobjects to narrowphase
      Assert::IsTrue(narrowphasePosX.at(0) == 1.1f || narrowphasePosX.at(0) == 1.2f);
    }

    TEST_METHOD(DistantPair_PopulateNarrowphase_NoPairs) {
      GameDatabase db;
      auto& gameobjects = std::get<GameObjectTable>(db.mTables);
      TableOperations::resizeTable(gameobjects, 2);
      auto& broadphase = std::get<BroadphaseTable>(db.mTables);
      auto& dimensions = std::get<SharedRow<GridBroadphase::RequestedDimensions>>(broadphase.mRows).at();
      dimensions.mMin.x = 0;
      dimensions.mMax.x = 10;
      dimensions.mMax.y = 9;
      dimensions.mMin.y = -1;
      auto& posX = std::get<FloatRow<Tags::Pos, Tags::X>>(gameobjects.mRows);
      auto& posY = std::get<FloatRow<Tags::Pos, Tags::Y>>(gameobjects.mRows);
      auto& pairs = std::get<CollisionPairsTable>(db.mTables);
      posX.at(0) = 1.0f;
      posX.at(1) = 5.0f;

      Physics::allocateBroadphase(broadphase);
      Physics::rebuildBroadphase(db.getTableIndex<GameObjectTable>().mValue, posX.mElements.data(), posY.mElements.data(), broadphase, size_t(2));
      Physics::generateCollisionPairs(broadphase, pairs);

      Assert::AreEqual(size_t(0), TableOperations::size(pairs));
    }

    TEST_METHOD(TwoPairsSameObject_GenerateCollisionPairs_HasPairs) {
      GameDatabase db;
      auto& gameobjects = std::get<GameObjectTable>(db.mTables);
      TableOperations::resizeTable(gameobjects, 3);
      auto& broadphase = std::get<BroadphaseTable>(db.mTables);
      auto& dimensions = std::get<SharedRow<GridBroadphase::RequestedDimensions>>(broadphase.mRows).at();
      dimensions.mMin.x = 0;
      dimensions.mMax.x = 10;
      dimensions.mMax.y = 9;
      dimensions.mMin.y = -1;
      auto& posX = std::get<FloatRow<Tags::Pos, Tags::X>>(gameobjects.mRows);
      auto& posY = std::get<FloatRow<Tags::Pos, Tags::Y>>(gameobjects.mRows);
      auto& pairs = std::get<CollisionPairsTable>(db.mTables);
      //This one to collide with both
      posX.at(0) = 5.0f;
      //This one to the left to collide with 1 but not 2
      posX.at(1) = 4.0f;
      //To the right, colliding with 0 but not 1
      posX.at(2) = 6.0f;

      Physics::allocateBroadphase(broadphase);
      Physics::rebuildBroadphase(db.getTableIndex<GameObjectTable>().mValue, posX.mElements.data(), posY.mElements.data(), broadphase, posX.size());
      Physics::generateCollisionPairs(broadphase, pairs);

      Assert::AreEqual(size_t(2), TableOperations::size(pairs));
      std::pair<size_t, size_t> a, b;
      auto pairA = TableOperations::getElement(pairs, 0);
      auto pairB = TableOperations::getElement(pairs, 1);
      //These asserts are enforcing a result order but that's not a requirement, rather it's easier to write the test that way
      Assert::AreEqual(size_t(0), pairB.get<0>());
      Assert::AreEqual(size_t(1), pairB.get<1>());
      Assert::AreEqual(size_t(0), pairA.get<0>());
      Assert::AreEqual(size_t(2), pairA.get<1>());
    }

    TEST_METHOD(CollidingPair_GenerateContacts_AreGenerated) {
      GameDatabase db;
      auto& gameobjects = std::get<GameObjectTable>(db.mTables);
      TableOperations::resizeTable(gameobjects, 2);
      auto& broadphase = std::get<BroadphaseTable>(db.mTables);
      auto& dimensions = std::get<SharedRow<GridBroadphase::RequestedDimensions>>(broadphase.mRows).at();
      dimensions.mMin.x = 0;
      dimensions.mMax.x = 10;
      dimensions.mMax.y = 9;
      dimensions.mMin.y = -1;
      auto& posX = std::get<FloatRow<Tags::Pos, Tags::X>>(gameobjects.mRows);
      auto& posY = std::get<FloatRow<Tags::Pos, Tags::Y>>(gameobjects.mRows);
      auto& cosAngle = std::get<FloatRow<Tags::Rot, Tags::CosAngle>>(gameobjects.mRows);
      cosAngle.at(0) = 1.0f;
      cosAngle.at(1) = 1.0f;
      auto& pairs = std::get<CollisionPairsTable>(db.mTables);
      const float expectedOverlap = 0.1f;
      posX.at(0) = 5.0f;
      posX.at(1) = 6.0f - expectedOverlap;

      Physics::allocateBroadphase(broadphase);
      Physics::rebuildBroadphase(db.getTableIndex<GameObjectTable>().mValue, posX.mElements.data(), posY.mElements.data(), broadphase, posX.size());
      Physics::generateCollisionPairs(broadphase, pairs);
      _fillNarrowphaseData(db);
      Physics::generateContacts(pairs);

      Assert::AreEqual(size_t(1), TableOperations::size(pairs));
      const float e = 0.00001f;
      Assert::AreEqual(expectedOverlap, std::get<ContactPoint<ContactOne>::Overlap>(pairs.mRows).at(0), e);
      Assert::AreEqual(5.5f, std::get<ContactPoint<ContactOne>::PosX>(pairs.mRows).at(0), e);
      Assert::AreEqual(-1.0f, std::get<SharedNormal::X>(pairs.mRows).at(0), e);
    }

    TEST_METHOD(CollidingPair_SolveConstraints_AreSeparated) {
      GameDatabase db;
      auto& gameobjects = std::get<GameObjectTable>(db.mTables);
      TableOperations::resizeTable(gameobjects, 2);
      auto& broadphase = std::get<BroadphaseTable>(db.mTables);
      auto& dimensions = std::get<SharedRow<GridBroadphase::RequestedDimensions>>(broadphase.mRows).at();
      auto& constraints = std::get<ConstraintsTable>(db.mTables);
      auto& pairs = std::get<CollisionPairsTable>(db.mTables);
      dimensions.mMin.x = 0;
      dimensions.mMax.x = 10;
      dimensions.mMax.y = 9;
      dimensions.mMin.y = -1;
      auto& cosAngle = std::get<FloatRow<Tags::Rot, Tags::CosAngle>>(gameobjects.mRows);
      cosAngle.at(0) = 1.0f;
      cosAngle.at(1) = 1.0f;
      auto& posX = std::get<FloatRow<Tags::Pos, Tags::X>>(gameobjects.mRows);
      auto& posY = std::get<FloatRow<Tags::Pos, Tags::Y>>(gameobjects.mRows);
      auto& velX = std::get<FloatRow<Tags::LinVel, Tags::X>>(gameobjects.mRows);
      const float expectedOverlap = 0.1f;
      posX.at(0) = 5.0f;
      velX.at(0) = 1.0f;
      posX.at(1) = 6.0f - expectedOverlap;
      velX.at(1) = -1.0f;

      Physics::allocateBroadphase(broadphase);
      Physics::rebuildBroadphase(db.getTableIndex<GameObjectTable>().mValue, posX.mElements.data(), posY.mElements.data(), broadphase, posX.size());
      Physics::generateCollisionPairs(broadphase, pairs);
      _fillNarrowphaseData(db);
      Physics::generateContacts(pairs);
      Physics::buildConstraintsTable(pairs, constraints);
      _fillConstraintVelocities(db);

      Physics::setupConstraints(constraints);
      Physics::solveConstraints(constraints);
      _storeConstraintVelocities(db);

      const float e = 0.001f;
      const float expectedBias = 0.1f;
      //Velocity of 0 going towards 1 should instead turn into a velocity of the two going away from each-other
      //by the overlap multiplied by the bias resolution amount. Since it's the relative velocity between the two,
      //each of them should have half of that bias amount
      const float expectedVelocity = expectedOverlap*expectedBias*0.5f;
      Assert::AreEqual(-expectedVelocity, velX.at(0), e);
      Assert::AreEqual(expectedVelocity, velX.at(1), e);
    }
  };
}