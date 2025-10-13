#include "Precompile.h"
#include "CppUnitTest.h"

#include "AppBuilder.h"
#include "Narrowphase.h"
#include "RuntimeDatabase.h"
#include "GameBuilder.h"
#include "GameScheduler.h"
#include "SpatialPairsStorage.h"
#include "TableAdapters.h"
#include <math/Geometric.h>
#include "PGSSolver.h"
#include "ConstraintSolver.h"
#include "TestApp.h"
#include "Physics.h"
#include "Dynamics.h"
#include <module/MassModule.h>
#include <module/PhysicsEvents.h>
#include <TestGame.h>
#include <PhysicsTableBuilder.h>
#include <math/AxisFlags.h>
#include <TableName.h>
#include <generics/Container.h>
#include <NotifyingTableModifier.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Test {
  TEST_CLASS(SolverTest) {
    static constexpr float E = 0.01f;
    struct StaticTag : TagRow {};
    struct DynamicTag : TagRow {};

    /*
    struct LinVelX : Row<float> {};
    struct LinVelY : Row<float> {};
    struct LinVelZ : Row<float> {};
    struct AngVel : Row<float> {};

    using DynamicObjects = Table<
      StableIDRow,
      LinVelX,
      LinVelY,
      LinVelZ,
      AngVel,
      ConstraintSolver::ConstraintMaskRow,
      MassModule::MassRow,
      ConstraintSolver::SharedMaterialRow
    >;

    using StaticObjects = Table<
      StaticTag,
      StableIDRow,
      ConstraintSolver::ConstraintMaskRow,
      ConstraintSolver::SharedMaterialRow
    >;

    using SolverDB = Database<
      DynamicObjects,
      StaticObjects,
      SP::SpatialPairsTable
    >;
    */
    struct TableIds {
      TableIds(RuntimeDatabaseTaskBuilder& task)
        : dynamicBodies{ task.query<DynamicTag>()[0] }
        , staticBodies{ task.query<StaticTag>()[0] }
        , spatialPairs{ task.query<SP::ManifoldRow>()[0] }
      {}

      TableID dynamicBodies, staticBodies, spatialPairs;
    };

    static StorageTableBuilder createDynamicTable() {
      StorageTableBuilder table;
      PhysicsTableBuilder::addRigidbody(table);
      PhysicsTableBuilder::addVelocity(table, math::AxisFlags::XYZA());
      table.setStable().addRows<DynamicTag>().setTableName({ "a" });
      return table;
    }

    static StorageTableBuilder createStaticTable() {
      StorageTableBuilder table;
      PhysicsTableBuilder::addRigidbody(table);
      PhysicsTableBuilder::addImmobile(table);
      table.setStable().addRows<StaticTag>().setTableName({ "b" });
      return table;
    }

    struct TestModule : IAppModule {
      void createDatabase(RuntimeDatabaseArgs& args) final {
        createDynamicTable().finalize(args);
        createStaticTable().finalize(args);
      }
    };

    struct SolverApp : TestGame {
      SolverApp()
        : TestGame{
            GameConstructArgs{
              .modules = gnx::Container::makeVector<std::unique_ptr<IAppModule>>(std::make_unique<TestModule>())
            }
          }
      {
        //initMTFromDB<SolverDB>([](IAppBuilder& builder) {
        //  static float bias = ConstraintSolver::SolverGlobals::BIAS_DEFAULT;
        //  static float slop = ConstraintSolver::SolverGlobals::SLOP_DEFAULT;
        //  ConstraintSolver::solveConstraints(builder, { &bias, &slop });
        //});
        //
        //builder().query<ConstraintSolver::ConstraintMaskRow>().forEachRow([](auto& row) {
        //  row.setDefaultValue(ConstraintSolver::MASK_SOLVE_ALL);
        //});
        //builder().query<ConstraintSolver::SharedMaterialRow>().forEachRow([](auto& row) {
        //  row.setDefaultValue(ConstraintSolver::Material{ 0, 0 });
        //});
      }

      ElementRef createInTable(TableID id) {
        NotifyingTableModifier modifier{ builder(), id };
        return *modifier.addElements(1);
      }
    };

    static void assertEq(const glm::vec2& l, const glm::vec2& r) {
      Assert::AreEqual(l.x, r.x, E);
      Assert::AreEqual(l.y, r.y, E);
    }

    TEST_METHOD(ConstraintSolver) {
      SolverApp app;
      auto& task = app.builder();
      const TableIds tables{ task };
      auto [islandGraph, manifold, pairA, pairB, pairTypeRow] = task.query<
        SP::IslandGraphRow,
        SP::ManifoldRow,
        SP::ObjA,
        SP::ObjB,
        SP::PairTypeRow
      >().get(0);
      IslandGraph::Graph& graph = islandGraph->at();
      auto [staticStableID] = task.query<StableIDRow>(tables.staticBodies).get(0);
      auto modifier = task.getModifierForTable(tables.spatialPairs);
      auto [dynamicStableId, dvx, dvy, dva] = task.query<
        StableIDRow,
        VelX,
        VelY,
        VelA
      >(tables.dynamicBodies).get(0);
      auto ids = task.getIDResolver();
      auto res = ids->getRefResolver();

      const ElementRef staticA = app.createInTable(tables.staticBodies);
      const ElementRef dynamicB = app.createInTable(tables.dynamicBodies);
      app.update();
      const size_t ib = res.uncheckedUnpack(dynamicB).getElementIndex();

      auto ar = staticA;
      auto br = dynamicB;
      IslandGraph::addNode(graph, ar);
      IslandGraph::addNode(graph, br);
      const size_t edgeAB = SP::addIslandEdge(*modifier, graph, *pairA, *pairB, *pairTypeRow, ar, br, SP::PairType::ContactXY);

      //TODO: This doesn't work with TestGame as narrowphase will generate the contacts
      //Probably easiest to use real shapes and let narrowphase actually compute the collisions.
      //Simulate B moving right towards A and hitting with two contact points on the corners of A
      dvx->at(ib) = 1.0f;
      SP::ContactManifold& manifoldAB = manifold->at(edgeAB);
      constexpr float overlap = 0.05f;
      manifoldAB.size = 2;
      manifoldAB[0].centerToContactB = { 0.5f, 0.5f };
      manifoldAB[0].normal = { 1, 0 };
      manifoldAB[0].overlap = overlap;
      manifoldAB[1].centerToContactB = { 0.5f, -0.5f };
      manifoldAB[1].normal = { 1, 0 };
      manifoldAB[1].overlap = overlap;

      app.update();

      {
        const glm::vec2 lv{ dvx->at(ib), dvy->at(ib) };
        const float av{ dva->at(ib) };
        for(int i = 0; i < 2; ++i) {
          const float relativeVelocity = glm::dot(manifoldAB[i].normal, Dyn::velocityAtPoint(manifoldAB[i].centerToContactB, lv, av));
          Assert::IsTrue(relativeVelocity <= 0.0f);
        }
      }

      //Simulate another object C moving downwards and colliding with A but not B
      const ElementRef dynamicC = app.createInTable(tables.dynamicBodies);
      const size_t ic = res.uncheckedUnpack(dynamicC).getElementIndex();
      auto cr = dynamicC;
      IslandGraph::addNode(graph, cr);
      const size_t edgeBC = SP::addIslandEdge(*modifier, graph, *pairA, *pairB, *pairTypeRow, br, cr, SP::PairType::ContactXY);

      dvy->at(ic) = -0.75f;

      SP::ContactManifold& manifoldBC = manifold->at(edgeBC);
      manifoldBC.size = 1;
      manifoldBC[0].centerToContactA = { 0.5f, 0.5f };
      manifoldBC[0].centerToContactB = { 0.5f, -0.5f };
      manifoldBC[0].normal = { 0, -1 };
      manifoldBC[0].overlap = overlap;

      app.update();

      {
        const glm::vec2 lvB{ dvx->at(ib), dvy->at(ib) };
        const float avB{ dva->at(ib) };
        const glm::vec2 lvC{ dvx->at(ic), dvy->at(ic) };
        const float avC{ dva->at(ic) };
        for(int i = 0; i < 2; ++i) {
          const float relativeVelocity = glm::dot(manifoldAB[i].normal, Dyn::velocityAtPoint(manifoldAB[i].centerToContactB, lvB, avB));
          Assert::IsTrue(relativeVelocity <= 0.0f);
        }
        //const float rv = glm::dot(manifoldBC[0].normal,
        //  Dyn::velocityAtPoint(manifoldBC[0].centerToContactA, lvB, avB) - Dyn::velocityAtPoint(manifoldBC[0].centerToContactB, lvC, avC));
        const glm::vec2 vpA = Dyn::velocityAtPoint(manifoldBC[0].centerToContactA, lvB, avB);
        const glm::vec2 vpB = Dyn::velocityAtPoint(manifoldBC[0].centerToContactB, lvC, avC);
        const float rvA = glm::dot(manifoldBC[0].normal, vpA);
        const float rvB = glm::dot(-manifoldBC[0].normal, vpB);
        const float rv = rvA + rvB;
        Assert::IsTrue(rv <= 0.0f);
      }
    }

    TEST_METHOD(ConstraintSolver3D) {
      SolverApp app;
      auto& task = app.builder();
      const TableIds tables{ task };
      auto [islandGraph, manifold, zManifold, pairA, pairB, pairTypeRow] = task.query<
        SP::IslandGraphRow,
        SP::ManifoldRow,
        SP::ZManifoldRow,
        SP::ObjA,
        SP::ObjB,
        SP::PairTypeRow
      >().get(0);
      auto modifier = task.getModifierForTable(tables.spatialPairs);
      IslandGraph::Graph& graph = islandGraph->at();
      auto [dynamicStableId, dvz, dva] = task.query<
        StableIDRow,
        VelZ,
        VelA
      >().get(0);
      auto ids = task.getIDResolver();
      auto res = ids->getRefResolver();

      const ElementRef a = app.createInTable(tables.dynamicBodies);
      const ElementRef b = app.createInTable(tables.dynamicBodies);

      const size_t ai = res.uncheckedUnpack(a).getElementIndex();
      const size_t bi = res.uncheckedUnpack(b).getElementIndex();

      auto ar = a;
      auto br = b;
      IslandGraph::addNode(graph, ar);
      IslandGraph::addNode(graph, br);
      const size_t edgeAB = SP::addIslandEdge(*modifier, graph, *pairA, *pairB, *pairTypeRow, ar, br, SP::PairType::ContactZ);

      //A moving up towards B but not fast enough to collide
      dvz->at(ai) = 1.0f;
      dvz->at(bi) = -1.0f;
      SP::ZContactManifold& manifoldAB = zManifold->at(edgeAB);
      manifoldAB.info.normal = -1.0f;
      manifoldAB.info.separation = 4.f;

      app.update();

      //A moving up towards B and going to collide
      constexpr float e = 0.01f;
      Assert::AreEqual(1.0f, dvz->at(ai), e, L"No collision so velocity should be unchanged");
      Assert::AreEqual(-1.0f, dvz->at(bi), e);

      manifoldAB.info.normal = -1.0f;
      manifoldAB.info.separation = 1.0f;

      app.update();

      Assert::AreEqual(0.5f, dvz->at(ai), e, L"Velocity should be reduced to prevent collision");
      Assert::AreEqual(-0.5f, dvz->at(bi), e);

      //A overlapping with B
      manifoldAB.info.normal = 1.0f;
      manifoldAB.info.separation = -0.1f;

      app.update();

      const float relativeVelocity = dvz->at(ai) - dvz->at(bi);
      Assert::IsTrue(relativeVelocity > 0.0f, L"Objects should be separating if they were overlapping");
    }

    TEST_METHOD(TwoBodiesLinearVelocity) {
      using namespace PGS;
      SolverStorage storage;
      storage.resize(2, 1);
      storage.setUniformMass(1, 0);
      storage.setUniformLambdaBounds(SolverStorage::UNLIMITED_MIN, SolverStorage::UNLIMITED_MAX);
      storage.setJacobian(0, 0, 1, { 1, 0 }, 0, { -1, 0 }, 0);
      const glm::vec2 va{ 5, 1 };
      const glm::vec2 vb{ -10, -2 };
      storage.setVelocity(0, va, 0);
      storage.setVelocity(1, vb, 0);

      storage.premultiply();

      SolveContext context{ storage.createContext() };

      solvePGS(context);

      Assert::AreEqual(0.0f, context.velocity.getObjectVelocity(0)[0] - context.velocity.getObjectVelocity(1)[0], E);
      Assert::AreEqual(va.y, context.velocity.getObjectVelocity(0)[1], E);
      Assert::AreEqual(vb.y, context.velocity.getObjectVelocity(1)[1], E);

      //Reset velocity and try warm start
      storage.setVelocity(0, va, 0);
      storage.setVelocity(1, vb, 0);

      //No iterations should be needed because warm start should get it right
      context.maxIterations = 0;
      solvePGSWarmStart(context);

      Assert::AreEqual(0.0f, context.velocity.getObjectVelocity(0)[0] - context.velocity.getObjectVelocity(1)[0], E);
      Assert::AreEqual(va.y, context.velocity.getObjectVelocity(0)[1], E);
      Assert::AreEqual(vb.y, context.velocity.getObjectVelocity(1)[1], E);
    }

    TEST_METHOD(ThreeBodiesLinearVelocity) {
      using namespace PGS;
      SolverStorage storage;
      storage.resize(3, 3);
      storage.setUniformMass(1, 0);
      storage.setUniformLambdaBounds(SolverStorage::UNLIMITED_MIN, SolverStorage::UNLIMITED_MAX);
      //A B
      //C
      const uint8_t a = 0;
      const uint8_t b = 1;
      const uint8_t c = 2;
      const glm::vec2 axisAB{ 1, 0 };
      const glm::vec2 axisAC{ 0, -1 };
      const glm::vec2 axisCB{ glm::normalize(glm::vec2(1, 1 )) };
      storage.setJacobian(0, a, b, axisAB, 0, -axisAB, 0);
      storage.setJacobian(1, a, c, axisAC, 0, -axisAC, 0);
      storage.setJacobian(2, c, b, axisCB, 0, -axisCB, 0);
      const glm::vec2 va{ 5, 1 };
      const glm::vec2 vb{ -10, -2 };
      const glm::vec2 vc{ -2, 1 };
      storage.setVelocity(0, va, 0);
      storage.setVelocity(1, vb, 0);
      storage.setVelocity(2, vc, 0);

      storage.premultiply();

      SolveContext context{ storage.createContext() };

      solvePGS(context);

      auto assertSolved = [&] {
        BodyVelocity bodyA{ context.velocity.getBody(a) };
        BodyVelocity bodyB{ context.velocity.getBody(b) };
        BodyVelocity bodyC{ context.velocity.getBody(c) };
        const float errorAB = glm::dot(axisAB, bodyA.linear) + glm::dot(-axisAB, bodyB.linear);
        const float errorAC = glm::dot(axisAC, bodyA.linear) + glm::dot(-axisAC, bodyC.linear);
        const float errorCB = glm::dot(axisCB, bodyC.linear) + glm::dot(-axisCB, bodyB.linear);
        Assert::AreEqual(0, errorAB, E);
        Assert::AreEqual(0, errorAC, E);
        Assert::AreEqual(0, errorCB, E);
      };
      assertSolved();

      storage.setVelocity(0, va, 0);
      storage.setVelocity(1, vb, 0);
      storage.setVelocity(2, vc, 0);

      context.maxIterations = 0;
      solvePGSWarmStart(context);

      assertSolved();
    }

    TEST_METHOD(LargeCounts) {
      using namespace PGS;
      SolverStorage storage;
      constexpr auto CONSTRAINTS = std::numeric_limits<uint16_t>::max();
      constexpr auto BODIES = std::numeric_limits<uint16_t>::max();
      storage.resize(BODIES, CONSTRAINTS);
      storage.setUniformMass(1, 0);
      storage.setUniformLambdaBounds(SolverStorage::UNLIMITED_MIN, SolverStorage::UNLIMITED_MAX);
      //Don't care what it does just want to make sure it doesn't get stuck in an infinite loop
      storage.premultiply();
      auto context = storage.createContext();
      PGS::solvePGSWarmStart(context);
    }

    //TODO, test case for bias and limits
  };
}