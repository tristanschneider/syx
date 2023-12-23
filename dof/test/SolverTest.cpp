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

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Test {
  TEST_CLASS(SolverTest) {
    static constexpr float E = 0.01f;

    static void assertEq(const glm::vec2& l, const glm::vec2& r) {
      Assert::AreEqual(l.x, r.x, E);
      Assert::AreEqual(l.y, r.y, E);
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

    //TODO, test case for bias and limits
  };
}