#include "Precompile.h"
#include "CppUnitTest.h"

#include "AppBuilder.h"
#include "TestGame.h"
#include "SceneNavigator.h"
#include "RowTags.h"
#include "TableAdapters.h"
#include "stat/ConstraintStatEffect.h"
#include "TransformResolver.h"
#include "PhysicsSimulation.h"
#include "glm/glm.hpp"
#include "NotifyingTableModifier.h"
#include "Geometric.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Test {
  TEST_CLASS(ConstraintTest) {
    struct CommandArgs {
      CommandArgs(RuntimeDatabaseTaskBuilder&) {
      }

      AppTaskArgs* taskArgs{};
    };
    struct ICommand {
      virtual ~ICommand() = default;
      virtual void process(CommandArgs& args) = 0;
    };

    struct Scene : SceneNavigator::IScene {
      void init(IAppBuilder& builder) {
        auto task = builder.createTask();
        task.setName("init");
        auto q = task.query<const Tags::DynamicPhysicsObjectsTag,
          Tags::PosXRow, Tags::PosYRow>();
        Assert::IsFalse(q.matchingTableIDs.empty());
        NotifyingTableModifier modifier{ task, q.matchingTableIDs[0] };
        task.setCallback([=](AppTaskArgs& args) mutable {
          modifier.initTask(args);
          const ElementRef* base = modifier.addElements(2);
          auto&& [_, x, y] = q.get(0);
          const size_t a = modifier.toIndex(*base);
          const size_t b = a + 1;
          objectA = base[0];
          objectB = base[1];

          TableAdapters::write(a, glm::vec2{ 1, 5 }, *x, *y);
          TableAdapters::write(b, glm::vec2{ 1, 10 }, *x, *y);
        });

        builder.submitTask(std::move(task));
      }

      void update(IAppBuilder& builder) {
        auto task = builder.createTask();
        task.setName("update");
        CommandArgs args{ task };

        task.setCallback([=](AppTaskArgs& taskArgs) mutable {
          args.taskArgs = &taskArgs;
          for(auto& cmd : commands) {
            cmd->process(args);
          }
          commands.clear();
        });

        builder.submitTask(std::move(task));
      }

      ElementRef objectA;
      ElementRef objectB;
      std::vector<std::shared_ptr<ICommand>> commands;
    };

    struct Game {
      Game() {
        game.update();
      }

      TestGame* operator->() {
        return &game;
      }

      TestGame& operator*() {
        return game;
      }

      std::unique_ptr<Scene> temp = std::make_unique<Scene>();
      Scene* scene = temp.get();
      TestGame game{ std::move(temp) };
      AppTaskArgs args{ game.sharedArgs() };
      ConstraintStatEffect::Builder constraintStat{ args };
    };

    struct SolveTimepoint {
      pt::Transform ta, tb;
      glm::vec2 worldA, worldB;
      float error{};
    };

    struct Solver {
      Game& game;
      const ElementRef a, b;
      const glm::vec2 localCenterToPinA, localCenterToPinB;
    };

    using ErrorFN = std::function<float(const SolveTimepoint&, const Solver&)>;

    static ErrorFN expectPointsMatchDistance(float distance) {
      return [distance](const SolveTimepoint& timepoint, const Solver&) {
        return std::abs(glm::distance(timepoint.worldA, timepoint.worldB) - distance);
      };
    }

    static ErrorFN expectOrientationIsWithinAngle(float angle) {
      return [angle](const SolveTimepoint& timepoint, const Solver& solver) {
        const glm::vec2 referenceA = timepoint.ta.transformVector(solver.localCenterToPinA);
        const glm::vec2 referenceB = -timepoint.tb.transformVector(solver.localCenterToPinB);
        const float cosAngle = glm::dot(referenceA, referenceB);
        return cosAngle > 0.0f ? std::acos(cosAngle) : Geo::PI2 + std::acos(-cosAngle);
      };
    }

    static ErrorFN expectAnd(const ErrorFN& fnA, const ErrorFN& fnB) {
      return [fnA, fnB](const SolveTimepoint& timepoint, const Solver& solver) {
        return fnA(timepoint, solver) + fnB(timepoint, solver);
      };
    }

    static ErrorFN expectWithinDistanceAndAngle(float distance, float angle) {
      return expectAnd(expectPointsMatchDistance(distance), expectOrientationIsWithinAngle(angle));
    }

    std::vector<SolveTimepoint> trySolve(const Solver& solver, size_t maxIterations, const ErrorFN& computeError) {
      std::vector<SolveTimepoint> result;
      result.reserve(maxIterations);
      pt::TransformResolver transform{ solver.game->builder(), PhysicsSimulation::getPhysicsAliases() };
      for(size_t i = 0; i < maxIterations; ++i) {
        solver.game->update();
        const pt::Transform ta = transform.resolve(solver.a);
        const pt::Transform tb = transform.resolve(solver.b);
        SolveTimepoint solve{
          .ta = ta,
          .tb = tb,
          .worldA = ta.transformPoint(solver.localCenterToPinA),
          .worldB = tb.transformPoint(solver.localCenterToPinB),
        };
        solve.error = computeError(solve, solver);
        result.push_back(solve);
      }
      return result;
    }

    void assertSolved(const std::vector<SolveTimepoint>& history) {
      Assert::AreEqual(0.0f, history.back().error, 0.01f);
      //If something weird happened both transforms could be zero which should still fail the test
      Assert::AreNotEqual(0.0f, glm::length(history.back().worldA), 0.01f);
    }

    TEST_METHOD(StatPinJoint1D) {
      Game game;
      const ElementRef a = game.scene->objectA;
      const ElementRef b = game.scene->objectB;
      constexpr size_t lifetime = 2000;
      Constraints::PinJoint1D joint{ .localCenterToPinA{ 1.0f, 0.0f }, .localCenterToPinB{ -1.0f, 0.0f } };
      pt::TransformResolver transform{ game->builder(), PhysicsSimulation::getPhysicsAliases() };

      game.constraintStat.createStatEffects(1).setOwner(a).setLifetime(lifetime);
      game.constraintStat.constraintBuilder().setJointType({ joint }).setTargets(a, b);

      Solver solver{ game, a, b, joint.localCenterToPinA, joint.localCenterToPinB };
      std::vector<SolveTimepoint> history = trySolve(solver, 300, expectPointsMatchDistance(0.0f));

      assertSolved(history);
    }

    TEST_METHOD(StatPinJointDistance1D) {
      Game game;
      const ElementRef a = game.scene->objectA;
      const ElementRef b = game.scene->objectB;
      constexpr size_t lifetime = 2000;
      Constraints::PinJoint1D joint{ .localCenterToPinA{ 1.0f, 0.0f }, .localCenterToPinB{ -1.0f, 0.0f }, .distance{ 1.0f } };
      pt::TransformResolver transform{ game->builder(), PhysicsSimulation::getPhysicsAliases() };

      game.constraintStat.createStatEffects(1).setOwner(a).setLifetime(lifetime);
      game.constraintStat.constraintBuilder().setJointType({ joint }).setTargets(a, b);

      Solver solver{ game, a, b, joint.localCenterToPinA, joint.localCenterToPinB };
      std::vector<SolveTimepoint> history = trySolve(solver, 400, expectPointsMatchDistance(1.0f));

      assertSolved(history);
    }

    TEST_METHOD(StatPinJoint2D) {
      Game game;
      const ElementRef a = game.scene->objectA;
      const ElementRef b = game.scene->objectB;
      constexpr size_t lifetime = 2000;
      Constraints::PinJoint2D joint{ .localCenterToPinA{ 1.0f, 0.0f }, .localCenterToPinB{ -1.0f, 0.0f } };
      pt::TransformResolver transform{ game->builder(), PhysicsSimulation::getPhysicsAliases() };

      game.constraintStat.createStatEffects(1).setOwner(a).setLifetime(lifetime);
      game.constraintStat.constraintBuilder().setJointType({ joint }).setTargets(a, b);

      Solver solver{ game, a, b, joint.localCenterToPinA, joint.localCenterToPinB };
      std::vector<SolveTimepoint> history = trySolve(solver, 250, expectPointsMatchDistance(0.0f));

      assertSolved(history);
    }

    TEST_METHOD(StatPinJointDistance2D) {
      Game game;
      const ElementRef a = game.scene->objectA;
      const ElementRef b = game.scene->objectB;
      constexpr size_t lifetime = 2000;
      Constraints::PinJoint2D joint{ .localCenterToPinA{ 1.0f, 0.0f }, .localCenterToPinB{ -1.0f, 0.0f }, .distance{ 1.0f } };
      pt::TransformResolver transform{ game->builder(), PhysicsSimulation::getPhysicsAliases() };

      game.constraintStat.createStatEffects(1).setOwner(a).setLifetime(lifetime);
      game.constraintStat.constraintBuilder().setJointType({ joint }).setTargets(a, b);

      Solver solver{ game, a, b, joint.localCenterToPinA, joint.localCenterToPinB };
      std::vector<SolveTimepoint> history = trySolve(solver, 250, expectPointsMatchDistance(1.0f));

      assertSolved(history);
    }

    TEST_METHOD(StatWeldJoint) {
      Game game;
      const ElementRef a = game.scene->objectA;
      const ElementRef b = game.scene->objectB;
      constexpr size_t lifetime = 2000;
      Constraints::WeldJoint joint{ .localCenterToPinA{ 1.0f, 0.0f }, .localCenterToPinB{ -1.0f, 0.0f } };
      pt::TransformResolver transform{ game->builder(), PhysicsSimulation::getPhysicsAliases() };

      game.constraintStat.createStatEffects(1).setOwner(a).setLifetime(lifetime);
      game.constraintStat.constraintBuilder().setJointType({ joint }).setTargets(a, b);

      Solver solver{ game, a, b, joint.localCenterToPinA, joint.localCenterToPinB };
      std::vector<SolveTimepoint> history = trySolve(solver, 250, expectWithinDistanceAndAngle(0.0f, 0.0f));

      assertSolved(history);
    }

    TEST_METHOD(StatWeldJointAngle) {
      Game game;
      const ElementRef a = game.scene->objectA;
      const ElementRef b = game.scene->objectB;
      constexpr size_t lifetime = 2000;
      Constraints::WeldJoint joint{ .localCenterToPinA{ 1.0f, 0.0f }, .localCenterToPinB{ -1.0f, 0.0f }, .allowedRotationRad{ 1.0f } };
      pt::TransformResolver transform{ game->builder(), PhysicsSimulation::getPhysicsAliases() };

      game.constraintStat.createStatEffects(1).setOwner(a).setLifetime(lifetime);
      game.constraintStat.constraintBuilder().setJointType({ joint }).setTargets(a, b);

      Solver solver{ game, a, b, joint.localCenterToPinA, joint.localCenterToPinB };
      std::vector<SolveTimepoint> history = trySolve(solver, 250, expectWithinDistanceAndAngle(0.0f, 1.0f));

      assertSolved(history);
    }

    //TODO: one sided constraint
    //TODO: configuring custom constraint using a scene task
  };
}