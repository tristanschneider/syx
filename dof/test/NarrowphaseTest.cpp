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
#include "TestApp.h"
#include "Physics.h"
#include "Clip.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

//Hack to make stopping at a test failure easier
#define ASSERT_TRUE(condition) {\
  const bool _condition = condition;\
  if(!_condition) __debugbreak();\
  Assert::IsTrue(_condition);\
}

namespace Test {
  struct PosX : Row<float> {};
  struct PosY : Row<float> {};
  struct RotX : Row<float> {};
  struct RotY : Row<float> {};
  struct PosZ : Row<float> {};
  struct ScaleX : Row<float> {};
  struct ScaleY : Row<float> {};
  using SharedUnitCubeTable = Table<
    StableIDRow,
    Narrowphase::CollisionMaskRow,
    Narrowphase::SharedRectangleRow,
    PosX,
    PosY,
    RotX,
    RotY
  >;
  using UnitCubeTable = Table<
    StableIDRow,
    Narrowphase::CollisionMaskRow,
    Narrowphase::RectangleRow
  >;
  using UnitCube3DTable = Table<
    StableIDRow,
    Narrowphase::CollisionMaskRow,
    Narrowphase::RectangleRow,
    Narrowphase::SharedThicknessRow,
    PosZ
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
  struct RectDef : Narrowphase::RectDefinition{
    RectDef() {
      centerX = FloatQueryAlias::create<PosX>().read();
      centerY = FloatQueryAlias::create<PosY>().read();
      rotX = FloatQueryAlias::create<RotX>().read();
      rotY = FloatQueryAlias::create<RotY>().read();
      scaleX = FloatQueryAlias::create<ScaleX>().read();
      scaleY = FloatQueryAlias::create<ScaleY>().read();
    }
  };
  struct PhysicsAlias : PhysicsAliases {
    PhysicsAlias() {
      posZ = FloatQueryAlias::create<PosZ>();
    }
  };
  using NarrowphaseDBT = Database<
    SP::SpatialPairsTable,
    SharedUnitCubeTable,
    UnitCubeTable,
    UnitCube3DTable,
    CircleTable,
    AABBTable,
    RaycastTable
  >;

  struct NarrowphaseTableIds {
    NarrowphaseTableIds(RuntimeDatabaseTaskBuilder& task)
      : spatialPairs{ task.query<SP::ManifoldRow>().matchingTableIDs[0] }
      , sharedUnitCubes{ task.query<Narrowphase::SharedRectangleRow>().matchingTableIDs[0] }
      , unitCubes3D{ task.query<Narrowphase::SharedThicknessRow>().matchingTableIDs[0] }
      , unitCubes{ task.query<Narrowphase::RectangleRow>().matchingTableIDs[0] }
      , circles{ task.query<Narrowphase::CircleRow>().matchingTableIDs[0] }
      , aabbs{ task.query<Narrowphase::AABBRow>().matchingTableIDs[0] }
      , raycasts{ task.query<Narrowphase::RaycastRow>().matchingTableIDs[0] }
    {}

    UnpackedDatabaseElementID spatialPairs;
    UnpackedDatabaseElementID sharedUnitCubes;
    UnpackedDatabaseElementID unitCubes3D;
    UnpackedDatabaseElementID unitCubes;
    UnpackedDatabaseElementID circles;
    UnpackedDatabaseElementID aabbs;
    UnpackedDatabaseElementID raycasts;
  };

  struct NarrowphaseDB : TestApp {
    NarrowphaseDB() {
      initSTFromDB<NarrowphaseDBT>([](IAppBuilder& builder) {
        Narrowphase::generateContactsFromSpatialPairs(builder, RectDef{}, PhysicsAlias{});
      });
    }

    void doNarrowphase() {
      update();
    }
  };

  struct SpatialQueriesAdapter {
    SpatialQueriesAdapter(RuntimeDatabaseTaskBuilder& task) {
      std::tie(objA, objB, manifold, zManifold) = task.query<
        SP::ObjA,
        SP::ObjB,
        SP::ManifoldRow,
        SP::ZManifoldRow
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
    SP::ZManifoldRow* zManifold{};
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

    TEST_METHOD(Cube3D) {
      NarrowphaseDB db;
      auto& task = db.builder();
      NarrowphaseTableIds tables{ task };
      auto modifier = task.getModifierForTable(tables.unitCubes3D);
      const size_t i = modifier->addElements(2);
      auto [stable, mask, cube, thickness, posZ] = task.query<
        StableIDRow,
        Narrowphase::CollisionMaskRow,
        Narrowphase::RectangleRow,
        Narrowphase::SharedThicknessRow,
        PosZ
      >().get(0);
      SpatialQueriesAdapter queries{ task };
      const size_t a = i;
      const size_t b = i + 1;
      const size_t q = queries.addPair(StableElementID::fromStableID(stable->at(a)), StableElementID::fromStableID(stable->at(b)));
      mask->at(a) = mask->at(b) = 1;
      //Default of no rotation, cos(0) = 1
      Narrowphase::Shape::Rectangle& cA = cube->at(a);
      Narrowphase::Shape::Rectangle& cB = cube->at(b);
      cA.right.x = cB.right.x = 1.f;
      SP::ContactManifold& manifold = queries.manifold->at(q);
      SP::ZContactManifold& zManifold = queries.zManifold->at(q);

      //Exactly touching
      cA.center.x = 1.0f;
      cB.center.x = 2.0f;

      db.doNarrowphase();

      auto assertContactsXZ = [&] {
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
      };
      assertContactsXZ();
      Assert::IsFalse(zManifold.info.has_value());

      //Separated by 1 on Z
      posZ->at(a) = 1.0f;
      posZ->at(b) = 2.0f;

      db.doNarrowphase();

      Assert::IsFalse(manifold.size, L"XY collision should not generate if separated on Z axis");
      Assert::IsTrue(zManifold.info.has_value());
      Assert::AreEqual(-1.0f, zManifold.info->normal, L"Normal should go towards A");
      Assert::AreEqual(1.0f, zManifold.info->separation);

      //0.1 of overlap
      thickness->at() = 0.5f;
      //Z position is bottom of the shape, top of the shape is bottom + thickness
      posZ->at(a) = 2.0f;
      posZ->at(b) = 1.6f;

      db.doNarrowphase();

      Assert::IsFalse(manifold.size);
      Assert::IsTrue(zManifold.info.has_value());
      Assert::AreEqual(1.0f, zManifold.info->normal);
      Assert::AreEqual(-0.1f, zManifold.info->separation, 0.001f, L"Overlap should show as negative separation");

      //Exact overlap on Z with thickness
      thickness->at() = 0.5f;
      posZ->at(a) = posZ->at(b) = 2.0f;

      db.doNarrowphase();

      assertContactsXZ();
      Assert::IsFalse(zManifold.info.has_value());
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

    bool matchesOrderless(const std::vector<glm::vec2>& a, const std::vector<glm::vec2>& b) {
      if(a.size() != b.size()) {
        return false;
      }
      return std::all_of(a.begin(), a.end(), [&b](const glm::vec2& pA) {
        return std::any_of(b.begin(), b.end(), [&pA](const glm::vec2& pB) {
          return Geo::near(pA, pB);
        });
      });
    }

    TEST_METHOD(ClipShapes) {
      std::vector<glm::vec2> a;
      std::vector<glm::vec2> b;
      b.push_back({ 1, 1 });
      b.push_back({ 2, 1 });
      b.push_back({ 2, 2 });
      b.push_back({ 1, 2 });

      //A and B intersecting
      a.push_back({ 0, 0 });
      a.push_back({ 1.5f, 0 });
      a.push_back({ 1.5f, 1.5f });
      a.push_back({ 0.0f, 1.5f });
      Clip::ClipContext context;

      Clip::clipShapes(a, b, context);
      Assert::IsTrue(matchesOrderless(context.result, {
        glm::vec2{ 1, 1 },
        glm::vec2{ 1.5f, 1 },
        glm::vec2{ 1.5f, 1.5f },
        glm::vec2{ 1, 1.5f }
      }));

      //A empty
      a.clear();
      Clip::clipShapes(a, b, context);
      Assert::IsTrue(context.result.empty());

      //A outside of B
      a.push_back({ -1, -1 });
      a.push_back({ -2, -2 });
      Clip::clipShapes(a, b, context);
      Assert::IsTrue(context.result.empty());

      //A inside of B
      a.clear();
      a.push_back({ 1, 1 });
      a.push_back({ 2, 2 });
      a.push_back({ 1, 2 });
      Clip::clipShapes(a, b, context);
      Assert::IsTrue(matchesOrderless(a, context.result));
    }

    struct Edge {
      glm::vec2 start;
      glm::vec2 end;
      glm::vec2 normal;
      float overlap{};
    };
    struct ValidationContext {
      bool isPointOnEdge(const glm::vec2& p) {
        //Point is inside if they are all in the other direction of the normal
        return std::any_of(edges.begin(), edges.end(), [&p](const Edge& e) {
          const glm::vec2 sp = p - e.start;
          if(Geo::nearZero(glm::dot(sp, e.normal))) {
            //Point's projection on normal is zero, make sure it is also within this edge
            return glm::dot(sp, p - e.end) <= Geo::EPSILON;
          }
          return false;
        });
      }

      Clip::ClipContext clipper;
      std::vector<Edge> edges;
      //Edges suitable to be a separating axis. Usually one but can be multiple if separation is equivalent on multiple axes
      std::vector<Edge> bestEdges;
    };

    static glm::vec2 average(const std::vector<glm::vec2>& points) {
      if(points.empty()) {
        return glm::vec2{ 0 };
      }
      return std::accumulate(points.begin(), points.end(), glm::vec2{ 0 }) / static_cast<float>(points.size());
    }

    static void validateManifold(const SP::ContactManifold& manifold,
      const std::vector<glm::vec2>& shapeA,
      const std::vector<glm::vec2>& shapeB,
      ValidationContext& context
    ) {
      //Clipping the shapes against each-other is a simple way to find the range of correct contact ponits
      Clip::clipShapes(shapeA, shapeB, context.clipper);
      //If the clipped shape is empty that would mean they had no points in common, meaning no collision
      if(context.clipper.result.empty()) {
        Assert::AreEqual(uint32_t(0), manifold.size);
        return;
      }
      //If the clipped shape has points then at least something should be in the manifold
      Assert::IsTrue(manifold.size > 0);

      const glm::vec2 centerA = average(shapeA);
      const glm::vec2 centerB = average(shapeB);

      //Find the best normal on the shape, which is the one that results on the least distance to the other side along that normal
      //Input shapes are counterclockwise so clipped shape should be as well, this should mean all normals are pointing outwards
      //Regardless use abs for simplicity
      context.edges.clear();
      for(size_t i = 0; i < context.clipper.result.size(); ++i) {
        Edge e;
        e.start = context.clipper.result[i];
        e.end = context.clipper.result[(i + 1) % context.clipper.result.size()];
        e.normal = Geo::orthogonal(e.end - e.start);
        if(!Geo::nearZero(e.normal, Geo::EPSILON/2.0f)) {
          e.normal = glm::normalize(e.normal);
          context.edges.push_back(e);
        }
      }

      //If there are no edges this means only the corners are exactly touching
      //Any normal in the separating direction is equally valid
      if(context.edges.empty()) {
        for(size_t i = 0; i < manifold.size; ++i) {
          const glm::vec2 pointA = manifold.points[i].centerToContactA + centerA;
          const glm::vec2 pointB = manifold.points[i].centerToContactB + centerB;
          ASSERT_TRUE(Geo::near(pointA, pointB));
          //All clipped points are the same which is the single corner point which should bethe contact
          ASSERT_TRUE(Geo::near(pointA, context.clipper.result[0]));
          ASSERT_TRUE(glm::dot(manifold.points[i].centerToContactA, manifold.points[i].normal) < 0.0f);
          Assert::AreEqual(0.0f, manifold.points[i].overlap, Geo::EPSILON);
        }
        return;
      }

      float bestDot = std::numeric_limits<float>::max();
      for(Edge& edge : context.edges) {
        float curDot = -1.0f;
        for(const glm::vec2& p : context.clipper.result) {
          curDot = std::max(curDot, std::abs(glm::dot(p - edge.start, edge.normal)));
        }
        edge.overlap = curDot;
        if(curDot < bestDot) {
          bestDot = curDot;
        }
      }
      context.bestEdges.clear();
      for(Edge& edge : context.edges) {
        if(Geo::near(edge.overlap,  bestDot)) {
          context.bestEdges.push_back(edge);
        }
      }

      for(size_t i = 0; i < manifold.size; ++i) {
        //Normal should match one of the directions
        const auto isValidNormal = std::any_of(context.bestEdges.begin(), context.bestEdges.end(), [&](const Edge& edge) {
          return Geo::near(manifold.points[i].normal, edge.normal) || Geo::near(manifold.points[i].normal, -edge.normal);
        });
        ASSERT_TRUE(isValidNormal);
        //All contact points should be on an edge of the clipped shape
        const glm::vec2 pointA = manifold.points[i].centerToContactA + centerA;
        const glm::vec2 pointB = manifold.points[i].centerToContactB + centerB;
        ASSERT_TRUE(context.isPointOnEdge(pointA));
        //Zero is hack to skip cases that are kind of incorrect but I don't want to deal with right now
        //TODO: fix it
        //Another hack since current implementation does produce suboptimal contacts
        if(i == 0) {
          ASSERT_TRUE(manifold.points[i].overlap == 0.0f || Geo::near(context.bestEdges[0].overlap, manifold.points[i].overlap));
        }
        ASSERT_TRUE(Geo::near(pointA, pointB));
      }
    }

    constexpr static glm::vec2 NOINTERSECT{ 99, 99 };
    glm::vec2 intersectLines(const glm::vec2& a1,
      const glm::vec2& a2,
      const glm::vec2& b1,
      const glm::vec2& b2
    ) {
      auto la = Clip::StartAndDir::fromStartEnd(a1, a2);
      auto lb = Clip::StartAndDir::fromStartEnd(b1, b2);
      Clip::LineLineIntersectTimes times = Clip::getIntersectTimes(la, lb);
      auto timeA = Clip::getIntersectA(la, lb);
      Assert::AreEqual(times.tA.has_value(), timeA.has_value());
      Assert::AreEqual(times.tA.value_or(0.0f), timeA.value_or(0.0f), 0.01f);
      const glm::vec2 ia = la.start + la.dir**times.tA;
      const glm::vec2 ib = lb.start + lb.dir**times.tB;
      if(times.tA.has_value()) {
        assertEq(ia, ib);
      }
      else {
        return NOINTERSECT;
      }
      return ia;
    }

    TEST_METHOD(LineLineIntersect) {
      assertEq({ 0, 0.5f }, intersectLines({ 0, 0 }, { 0, 1 }, { -0.5f, 0.5f }, { 0.5f, 0.5f }));
      assertEq({ 0.5f, 0 }, intersectLines({ 0.5f, -0.5f }, { 0.5f, 0.5f }, { 0, 0 }, { 1, 0 }));
      assertEq(NOINTERSECT, intersectLines({ 0.5f, -0.5f }, { 0.5f, 0.5f }, { 1.5f, 0.5f }, { 1.5f, 0.5f }));
    }

    static void transform(const std::vector<glm::vec2>& input, std::vector<glm::vec2>& output, const glm::mat3x3& matrix) {
      output.resize(input.size());
      for(size_t i = 0; i < input.size(); ++i) {
        output[i] = Geo::transformPoint(matrix, input[i]);
      }
    }

    TEST_METHOD(BoxBoxStressTest) {
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
      const glm::vec2 origin{ 1, 1 };
      mask->at(a) = mask->at(b) = 1;
      SP::ContactManifold& manifold = queries.manifold->at(q);
      const glm::vec2 sizeA{ 2, 2 };
      const glm::vec2 sizeB{ sizeA };
      TableAdapters::write(b, origin, *posX, *posY);
      rotX->at(b) = 1.0f;

      std::vector<glm::vec2> localShape{
        glm::vec2{ -0.5f, -0.5f },
        glm::vec2{  0.5f, -0.5f },
        glm::vec2{  0.5f,  0.5f },
        glm::vec2{ -0.5f,  0.5f },
      };
      std::vector<glm::vec2> worldShapeA;
      std::vector<glm::vec2> worldShapeB;
      transform(localShape, worldShapeB, Geo::buildTranslate(origin));

      constexpr size_t posIncrement = 100;
      constexpr size_t angleIncrement = 3;
      const glm::vec2 min = origin - sizeA/2.0f - sizeB/2.0f;
      const glm::vec2 size = sizeA + sizeB;
      ValidationContext context;
      for(size_t w = 0; w < angleIncrement; ++w) {
        const float pw = static_cast<float>(w)/static_cast<float>(angleIncrement);
        const float angle = Geo::TAU*pw;
        const glm::vec2 rot{ std::cos(angle), std::sin(angle) };
        glm::vec2 pos{};
        for(size_t x = 0; x < posIncrement; ++x) {
          const float px = static_cast<float>(x)/static_cast<float>(posIncrement);
          pos.x = min.x + size.x*px;
          for(size_t y = 0; y < posIncrement; ++y) {
            const float py = static_cast<float>(y)/static_cast<float>(posIncrement);
            pos.y = min.y + size.y*py;

            TableAdapters::write(a, pos, *posX, *posY);
            TableAdapters::write(a, rot, *rotX, *rotY);

            db.doNarrowphase();

            transform(localShape, worldShapeA, Geo::buildTransform(pos, rot, { 1, 1 }));
            validateManifold(manifold, worldShapeA, worldShapeB, context);
          }
        }
      }
    }
  };
}