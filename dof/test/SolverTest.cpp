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
#include "PGSSolver.h"
#include "ConstraintSolver.h"
#include "TestApp.h"
#include "Physics.h"
#include "Dynamics.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Test {
  TEST_CLASS(SolverTest) {
    static constexpr float E = 0.01f;

    struct LinVelX : Row<float> {};
    struct LinVelY : Row<float> {};
    struct LinVelZ : Row<float> {};
    struct AngVel : Row<float> {};

    using DynamicObjects = Table<
      StableIDRow,
      LinVelX,
      LinVelY,
      AngVel,
      ConstraintSolver::ConstraintMaskRow,
      ConstraintSolver::SharedMassRow,
      ConstraintSolver::SharedMaterialRow
    >;

    struct StaticTag : TagRow {};
    using StaticObjects = Table<
      StaticTag,
      StableIDRow,
      ConstraintSolver::ConstraintMaskRow,
      ConstraintSolver::SharedMassRow,
      ConstraintSolver::SharedMaterialRow
    >;

    using SolverDB = Database<
      DynamicObjects,
      StaticObjects,
      SP::SpatialPairsTable
    >;

    struct TableIds {
      TableIds(RuntimeDatabaseTaskBuilder& task)
        : dynamicBodies{ task.query<LinVelX>().matchingTableIDs[0] }
        , staticBodies{ task.query<StaticTag>().matchingTableIDs[0] }
        , spatialPairs{ task.query<SP::ManifoldRow>().matchingTableIDs[0] }
      {}

      UnpackedDatabaseElementID dynamicBodies, staticBodies, spatialPairs;
    };

    struct TestAliases : PhysicsAliases {
      TestAliases() {
        linVelX = FloatQueryAlias::create<LinVelX>();
        linVelY = FloatQueryAlias::create<LinVelY>();
        linVelZ = FloatQueryAlias::create<LinVelZ>();
        angVel = FloatQueryAlias::create<AngVel>();
      }
    };

    struct SolverApp : TestApp {
      SolverApp() {
        initMTFromDB<SolverDB>([](IAppBuilder& builder) {
          static float bias = ConstraintSolver::SolverGlobals::BIAS_DEFAULT;
          static float slop = ConstraintSolver::SolverGlobals::SLOP_DEFAULT;
          ConstraintSolver::solveConstraints(builder, TestAliases{}, { &bias, &slop });
        });
        TableIds ids{ builder() };
        ConstraintSolver::BodyMass* dynamicMass = builder().query<ConstraintSolver::SharedMassRow>(ids.dynamicBodies).tryGetSingletonElement();
        *dynamicMass = Geo::computeQuadMass(1, 1, 1);
        //Static mass is already zero as desired

        builder().query<ConstraintSolver::ConstraintMaskRow>().forEachRow([](auto& row) {
          row.setDefaultValue(ConstraintSolver::MASK_SOLVE_ALL);
        });
        builder().query<ConstraintSolver::SharedMaterialRow>().forEachRow([](auto& row) {
          row.setDefaultValue(ConstraintSolver::Material{ 0, 0 });
        });
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
      auto [islandGraph, manifold] = task.query<
        SP::IslandGraphRow,
        SP::ManifoldRow
      >().get(0);
      IslandGraph::Graph& graph = islandGraph->at();
      auto [staticStableID] = task.query<StableIDRow>(tables.staticBodies).get(0);
      auto [dynamicStableId, dvx, dvy, dva] = task.query<
        StableIDRow,
        LinVelX,
        LinVelY,
        AngVel
      >().get(0);
      auto ids = task.getIDResolver();

      const ResolvedIDs staticA = app.createInTable(tables.staticBodies);
      const ResolvedIDs dynamicB = app.createInTable(tables.dynamicBodies);
      const ResolvedIDs edgeAB = app.createInTable(tables.spatialPairs);
      const size_t ib = dynamicB.unpacked.getElementIndex();

      IslandGraph::addNode(graph, staticA.stable.mStableID);
      IslandGraph::addNode(graph, dynamicB.stable.mStableID);
      IslandGraph::addEdge(graph, staticA.stable.mStableID, dynamicB.stable.mStableID, edgeAB.stable.mStableID);

      //Simulate B moving right towards A and hitting with two contact points on the corners of A
      dvx->at(ib) = 1.0f;
      SP::ContactManifold& manifoldAB = manifold->at(edgeAB.unpacked.getElementIndex());
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
      const ResolvedIDs dynamicC = app.createInTable(tables.dynamicBodies);
      const ResolvedIDs edgeBC = app.createInTable(tables.spatialPairs);
      const size_t ic = dynamicC.unpacked.getElementIndex();
      IslandGraph::addNode(graph, dynamicC.stable.mStableID);
      IslandGraph::addEdge(graph, dynamicB.stable.mStableID, dynamicC.stable.mStableID, edgeBC.stable.mStableID);

      dvy->at(ic) = -0.75f;

      SP::ContactManifold& manifoldBC = manifold->at(edgeBC.unpacked.getElementIndex());
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
      constexpr auto CONSTRAINTS = std::numeric_limits<PGS::ConstraintIndex>::max();
      constexpr auto BODIES = std::numeric_limits<PGS::BodyIndex>::max();
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