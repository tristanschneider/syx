#include "Precompile.h"
#include "CppUnitTest.h"

#include "AppBuilder.h"
#include "Narrowphase.h"
#include "RuntimeDatabase.h"
#include "GameBuilder.h"
#include "GameScheduler.h"
#include "SpatialPairsStorage.h"
#include "TableAdapters.h"
#include "Geometric.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Test {
  struct PosX : Row<float> {};
  struct PosY : Row<float> {};
  struct RotX : Row<float> {};
  struct RotY : Row<float> {};
  using SharedUnitCubeTable = Table<
    StableIDRow,
    Narrowphase::CollisionMaskRow,
    Narrowphase::SharedUnitCubeRow,
    PosX,
    PosY,
    RotX,
    RotY
  >;
  using UnitCubeTable = Table<
    StableIDRow,
    Narrowphase::CollisionMaskRow,
    Narrowphase::UnitCubeRow
  >;
  using CircleTable = Table<
    StableIDRow,
    Narrowphase::CollisionMaskRow,
    Narrowphase::CircleRow
  >;
  using AABBTable = Table<
    StableIDRow,
    Narrowphase::CollisionMaskRow,
    Narrowphase::AABBRow
  >;
  using RaycastTable = Table<
    StableIDRow,
    Narrowphase::CollisionMaskRow,
    Narrowphase::RaycastRow
  >;
  struct UnitCubeDef : Narrowphase::UnitCubeDefinition {
    UnitCubeDef() {
      centerX = FloatQueryAlias::create<PosX>().read();
      centerY = FloatQueryAlias::create<PosY>().read();
      rotX = FloatQueryAlias::create<RotX>().read();
      rotY = FloatQueryAlias::create<RotY>().read();
    }
  };
  using NarrowphaseDBT = Database<
    SP::SpatialPairsTable,
    SharedUnitCubeTable,
    UnitCubeTable,
    CircleTable,
    AABBTable,
    RaycastTable
  >;

  struct NarrowphaseTableIds {
    NarrowphaseTableIds(RuntimeDatabaseTaskBuilder& task)
      : spatialPairs{ task.query<SP::ManifoldRow>().matchingTableIDs[0] }
      , sharedUnitCubes{ task.query<Narrowphase::SharedUnitCubeRow>().matchingTableIDs[0] }
      , unitCubes{ task.query<Narrowphase::UnitCubeRow>().matchingTableIDs[0] }
      , circles{ task.query<Narrowphase::CircleRow>().matchingTableIDs[0] }
      , aabbs{ task.query<Narrowphase::AABBRow>().matchingTableIDs[0] }
      , raycasts{ task.query<Narrowphase::RaycastRow>().matchingTableIDs[0] }
    {}

    UnpackedDatabaseElementID spatialPairs;
    UnpackedDatabaseElementID sharedUnitCubes;
    UnpackedDatabaseElementID unitCubes;
    UnpackedDatabaseElementID circles;
    UnpackedDatabaseElementID aabbs;
    UnpackedDatabaseElementID raycasts;
  };

  struct NarrowphaseDB {
    NarrowphaseDB() {
      auto mappings = std::make_unique<StableElementMappings>();
      db = DBReflect::createDatabase<NarrowphaseDBT>(*mappings);
      db = DBReflect::bundle(std::move(db), std::move(mappings));
      auto builder = GameBuilder::create(*db);
      auto temp = builder->createTask();
      temp.discard();
      taskBuilder = std::make_unique<RuntimeDatabaseTaskBuilder>(std::move(temp));
      taskBuilder->discard();

      Narrowphase::generateContactsFromSpatialPairs(*builder, UnitCubeDef{});

      work = GameScheduler::buildSync(IAppBuilder::finalize(std::move(builder)));
    }

    RuntimeDatabaseTaskBuilder& builder() {
      return *taskBuilder;
    }

    void doNarrowphase() {
      for(auto&& w : work) {
        w.work();
      }
    }

    std::vector<GameScheduler::SyncWorkItem> work;
    std::unique_ptr<IDatabase> db;
    std::unique_ptr<RuntimeDatabaseTaskBuilder> taskBuilder;
  };

  struct SpatialQueriesAdapter {
    SpatialQueriesAdapter(RuntimeDatabaseTaskBuilder& task) {
      std::tie(objA, objB, manifold) = task.query<
        SP::ObjA,
        SP::ObjB,
        SP::ManifoldRow
      >().get(0);
      modifier = task.getModifierForTable(task.query<SP::ManifoldRow>().matchingTableIDs[0]);
    }

    size_t addPair(const StableElementID& a, const StableElementID& b) {
      const size_t i = modifier->addElements(1);
      objA->at(i) = a;
      objB->at(i) = b;
      return i;
    }

    SP::ObjA* objA{};
    SP::ObjB* objB{};
    SP::ManifoldRow* manifold{};
    std::shared_ptr<ITableModifier> modifier;
  };

  TEST_CLASS(NarrowphaseTest) {
    static constexpr float E = 0.01f;

    static void assertEq(const glm::vec2& l, const glm::vec2& r) {
      Assert::AreEqual(l.x, r.x, E);
      Assert::AreEqual(l.y, r.y, E);
    }

    TEST_METHOD(CircleToCircle) {
      NarrowphaseDB db;
      auto& task = db.builder();
      NarrowphaseTableIds tables{ task };
      auto modifier = task.getModifierForTable(tables.circles);
      const size_t i = modifier->addElements(2);
      auto [stable, mask, circles] = task.query<
        StableIDRow,
        Narrowphase::CollisionMaskRow,
        Narrowphase::CircleRow
      >().get(0);
      SpatialQueriesAdapter queries{ task };
      const size_t a = i;
      const size_t b = i + 1;
      const size_t q = queries.addPair(StableElementID::fromStableID(stable->at(a)), StableElementID::fromStableID(stable->at(b)));
      mask->at(a) = mask->at(b) = 1;
      Narrowphase::Shape::Circle& circleA = circles->at(a);
      Narrowphase::Shape::Circle& circleB = circles->at(b);
      SP::ContactManifold& manifold = queries.manifold->at(q);
      const SP::ContactPoint& contact = manifold[0];

      //Exactly touching
      circleA.pos = { 1, 2 };
      circleA.radius = 1.0f;
      circleB.pos = { 4, 2 };
      circleB.radius = 2.0f;

      db.doNarrowphase();

      Assert::AreEqual(uint32_t{ 1 }, manifold.size);
      Assert::AreEqual(0.0f, contact.overlap, E);
      assertEq({ -1, 0 }, contact.normal);
      assertEq({ 1, 0 }, contact.centerToContactA);
      assertEq({ -2, 0 }, contact.centerToContactB);

      //Not touching
      circleB.pos.x = 4.1f;

      db.doNarrowphase();

      Assert::AreEqual(uint32_t{ 0 }, manifold.size);

      //Overlapping a bit
      circleB.pos.x = 3.9f;

      db.doNarrowphase();

      Assert::AreEqual(uint32_t{ 1 }, manifold.size);
      Assert::AreEqual(0.1f, contact.overlap, E);
      assertEq({ -1, 0 }, contact.normal);
      assertEq({ 1, 0 }, contact.centerToContactA);
      assertEq({ -1.9f, 0.f }, contact.centerToContactB);

      //Exactly overlapping
      circleA.pos = circleB.pos = { 0, 0 };

      db.doNarrowphase();

      Assert::AreEqual(uint32_t{ 1 }, manifold.size);
      Assert::AreEqual(3.f, contact.overlap, E);
      assertEq({ 1, 0 }, contact.normal);
      assertEq({ -1, 0 }, contact.centerToContactA);
      assertEq({ -1, 0 }, contact.centerToContactB);

      //Different collision layers
      mask->at(a) = 2;

      db.doNarrowphase();

      Assert::AreEqual(uint32_t{ 0 }, manifold.size);
    }

    static void assertHasPoints(const SP::ContactManifold& manifold, std::initializer_list<SP::ContactPoint> expected) {
      Assert::AreEqual(static_cast<uint32_t>(expected.size()), manifold.size);
      for(auto&& e : expected) {
        bool found = false;
        for(uint32_t i = 0; i < manifold.size; ++i) {
          const auto& a = manifold[i];
          if(Geo::near(e.centerToContactA, a.centerToContactA) &&
            Geo::near(e.centerToContactB, a.centerToContactB) &&
            Geo::near(e.normal, a.normal) &&
            Geo::near(e.overlap, a.overlap)
          ) {
            found = true;
            break;
          }
        }
        if(!found) {
          Assert::Fail();
        }
      }
    }

    TEST_METHOD(CubeCube) {
      NarrowphaseDB db;
      auto& task = db.builder();
      NarrowphaseTableIds tables{ task };
      auto modifier = task.getModifierForTable(tables.sharedUnitCubes);
      const size_t i = modifier->addElements(2);
      auto [stable, mask, posX, posY, rotX, rotY] = task.query<
        StableIDRow,
        Narrowphase::CollisionMaskRow,
        PosX,
        PosY,
        RotX,
        RotY
      >().get(0);
      SpatialQueriesAdapter queries{ task };
      const size_t a = i;
      const size_t b = i + 1;
      const size_t q = queries.addPair(StableElementID::fromStableID(stable->at(a)), StableElementID::fromStableID(stable->at(b)));
      mask->at(a) = mask->at(b) = 1;
      //Default of no rotation, cos(0) = 1
      rotX->at(a) = rotX->at(b) = 1.f;
      //Something nonzero so local vs world space bugs would fail tests
      posY->at(a) = posY->at(b) = 2.f;
      SP::ContactManifold& manifold = queries.manifold->at(q);

      //Exactly touching
      posX->at(a) = 1.0f;
      posX->at(b) = 2.0f;

      db.doNarrowphase();

      assertHasPoints(manifold, {
        SP::ContactPoint{
          { -1, 0 },
          { 0.5f, 0.5f },
          { -0.5f, 0.5f },
          0.f,
          0.f
        },
        SP::ContactPoint{
          { -1, 0 },
          { 0.5f, -0.5f },
          { -0.5f, -0.5f },
          0.f,
          0.f
        }
      });

      //Not touching
      posX->at(b) = 2.1f;

      db.doNarrowphase();

      Assert::AreEqual(uint32_t{ 0 }, manifold.size);

      //Overlapping a bit
      posX->at(b) = 1.9f;

      db.doNarrowphase();

      //Points are expected to be on the surface of A
      assertHasPoints(manifold, {
        SP::ContactPoint{
          { -1, 0 },
          { 0.4f, 0.5f },
          { -0.5f, 0.5f },
          0.1f,
          0.f
        },
        SP::ContactPoint{
          { -1, 0 },
          { 0.4f, -0.5f },
          { -0.5f, -0.5f },
          0.1f,
          0.f
        }
      });


      //Exactly overlapping
      posX->at(a) = posX->at(b) = 1.f;

      db.doNarrowphase();

      //Not particularly important what they find as long s the overlap amount is right
      Assert::AreEqual(uint32_t{ 2 }, manifold.size);
      Assert::AreEqual(1.0f, manifold[0].overlap, 0.001f);
      Assert::AreEqual(1.0f, manifold[1].overlap, 0.001f);

      //B at the top right corner of A at a 45 degree angle
      // _/_\
      //| \|/
      //|__|
      {
        constexpr float diagonal = 3.141f/4.0f;
        const glm::vec2 rot{ std::cos(diagonal), std::sin(diagonal) };
        const glm::vec2 aPos{ 1, 2 };
        const float cornerDistance = std::sqrt(0.5f*0.5f + 0.5f*0.5f);
        const glm::vec2 corner = aPos + glm::vec2{ 0.5f, 0.5f };
        const glm::vec2 bPos = corner;
        TableAdapters::write(a, aPos, *posX, *posY);
        TableAdapters::write(b, bPos, *posX, *posY);
        TableAdapters::write(b, rot, *rotX, *rotY);

        db.doNarrowphase();

        const glm::vec2 leftIntersect = corner - glm::vec2{ cornerDistance, 0.f };
        const glm::vec2 rightIntersect = corner - glm::vec2{ 0.f, cornerDistance };
        assertHasPoints(manifold, {
          SP::ContactPoint{
            -rot,
            leftIntersect - aPos,
            leftIntersect - bPos,
            0.5f,
            0.f
          },
          SP::ContactPoint{
            -rot,
            rightIntersect - aPos,
            rightIntersect - bPos,
            0.5f,
            0.f
          }
        });
      }
    }
  };
}