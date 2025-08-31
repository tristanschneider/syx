#include "Precompile.h"
#include "CppUnitTest.h"

#include "AppBuilder.h"
#include "Events.h"
#include "TestGame.h"
#include "SceneNavigator.h"
#include "RowTags.h"
#include "TableAdapters.h"
#include "stat/ConstraintStatEffect.h"
#include <transform/TransformResolver.h>
#include "PhysicsSimulation.h"
#include "glm/glm.hpp"
#include "NotifyingTableModifier.h"
#include <math/Geometric.h>
#include "VelocityResolver.h"
#include <transform/TransformRows.h>
#include <Physics.h>

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
        auto q = task.query<const Tags::DynamicPhysicsObjectsWithZTag,
          Transform::WorldTransformRow>();
        Assert::IsFalse(q.size() == 0);
        NotifyingTableModifier modifier{ task, q[0] };
        task.setCallback([=](AppTaskArgs& args) mutable {
          modifier.initTask(args);
          const ElementRef* base = modifier.addElements(2);
          auto&& [_, transforms] = q.get(0);
          const size_t a = modifier.toIndex(*base);
          const size_t b = a + 1;
          objectA = base[0];
          objectB = base[1];

          transforms->at(a).setPos(glm::vec2{ 1, 5 });
          transforms->at(b).setPos(glm::vec2{ 1, 10 });
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
        game.update();
      }

      TestGame* operator->() {
        return &game;
      }

      TestGame& operator*() {
        return game;
      }

      void awaitConstraintGC() {
        for(size_t i = 0; i < 101; ++i) {
          game.update();
        }
      }

      void markForDestruction(const ElementRef& e) {
        CachedRow<Events::EventsRow> events;
        ElementRefResolver ids = game.builder().getRefResolver();
        const auto id = ids.unpack(e);
        if(game.builder().getResolver(events)->tryGetOrSwapRow(events, id)) {
          events->getOrAdd(id.getElementIndex()).setDestroy();
        }
      }

      std::unique_ptr<Scene> temp = std::make_unique<Scene>();
      Scene* scene = temp.get();
      TestGame game{ std::move(temp) };
      AppTaskArgs& args{ game.sharedArgs() };
      ConstraintStatEffect::Builder constraintStat{ args };
    };

    struct SolveTimepoint {
      Transform::PackedTransform ta, tb;
      glm::vec2 worldA, worldB;
      pt::Velocities vA, vB;
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
        return cosAngle > 0.0f ? std::acos(cosAngle) : Constants::PI2 + std::acos(-cosAngle);
      };
    }

    static ErrorFN expectTargetLinearVelocity(const glm::vec2& target) {
      return [target](const SolveTimepoint& timepoint, const Solver&) {
        const glm::vec2 v = target - (timepoint.vA.linear - timepoint.vB.linear);
        return std::abs(glm::length(v));
      };
    }

    static ErrorFN expectTargetZVelocity(float target) {
      return [target](const SolveTimepoint& timepoint, const Solver&) {
        const float v = target - (timepoint.vA.linearZ - timepoint.vB.linearZ);
        return std::abs(v);
      };
    }

    static ErrorFN expectTargetAngularVelocity(float target) {
      return [target](const SolveTimepoint& timepoint, const Solver&) {
        const float v = target - (timepoint.vA.angular - timepoint.vB.angular);
        return std::abs(v);
      };
    }

    static ErrorFN expectOneSidedOrientation(float angle) {
      return [target{Geo::directionFromAngle(angle)}](const SolveTimepoint& timepoint, const Solver&) {
        return std::abs(1.0f - glm::dot(target, timepoint.ta.rot()));
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

    static ErrorFN expectTargetVelocity(const glm::vec2& linear, float angular) {
      return expectAnd(expectTargetLinearVelocity(linear), expectTargetAngularVelocity(angular));
    }

    std::vector<SolveTimepoint> trySolve(const Solver& solver, size_t maxIterations, const ErrorFN& computeError) {
      std::vector<SolveTimepoint> result;
      result.reserve(maxIterations);
      Transform::Resolver transform{ solver.game->builder(), {} };
      pt::VelocityResolver velocity{ solver.game->builder(), pt::ConstVelocities::create(PhysicsSimulation::getPhysicsAliases()) };
      for(size_t i = 0; i < maxIterations; ++i) {
        solver.game->update();
        const Transform::PackedTransform ta = transform.resolve(solver.a);
        const Transform::PackedTransform tb = transform.resolve(solver.b);
        SolveTimepoint solve{
          .ta = ta,
          .tb = tb,
          .worldA = ta.transformPoint(solver.localCenterToPinA),
          .worldB = tb.transformPoint(solver.localCenterToPinB),
          .vA = velocity.resolve(solver.a),
          .vB = velocity.resolve(solver.b)
        };
        solve.error = computeError ? computeError(solve, solver) : 0.0f;
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

      game.constraintStat.createStatEffects(1).setOwner(a).setLifetime(lifetime);
      game.constraintStat.constraintBuilder().setJointType({ joint }).setTargets(a, b);

      Solver solver{ game, a, b, joint.localCenterToPinA, joint.localCenterToPinB };
      std::vector<SolveTimepoint> history = trySolve(solver, 250, expectWithinDistanceAndAngle(0.0f, 1.0f));

      assertSolved(history);
    }

    TEST_METHOD(StatOneSidedMotorJointWorldAbsoluteRotation) {
      Game game;
      const ElementRef a = game.scene->objectA;
      constexpr size_t lifetime = 2000;
      Constraints::MotorJoint joint{ .linearTarget{ 1, 2 }, .angularTarget{ 0.5f }, .linearForce{ 1.0f }, .angularForce{ 1.0f } };
      joint.flags.set(gnx::enumCast(Constraints::MotorJoint::Flags::WorldSpaceLinear));
      joint.flags.set(gnx::enumCast(Constraints::MotorJoint::Flags::CanPull));

      game.constraintStat.createStatEffects(1).setOwner(a).setLifetime(lifetime);
      game.constraintStat.constraintBuilder().setJointType({ joint }).setTargets(a, {});

      Solver solver{ .game{ game }, .a{ a }, .b{}, .localCenterToPinA{}, .localCenterToPinB{} };
      std::vector<SolveTimepoint> history = trySolve(solver, 5, expectTargetVelocity(joint.linearTarget, joint.angularTarget));

      assertSolved(history);
    }

    TEST_METHOD(StatOneSidedMotorJointWorldTargetRotation) {
      Game game;
      const ElementRef a = game.scene->objectA;
      constexpr size_t lifetime = 2000;
      Constraints::MotorJoint joint{ .linearTarget{ 1, 2 }, .angularTarget{ 0.5f }, .linearForce{ 1.0f }, .angularForce{ 1.0f } };
      joint.flags.set(gnx::enumCast(Constraints::MotorJoint::Flags::WorldSpaceLinear));
      joint.flags.set(gnx::enumCast(Constraints::MotorJoint::Flags::AngularOrientationTarget));
      joint.flags.set(gnx::enumCast(Constraints::MotorJoint::Flags::CanPull));

      game.constraintStat.createStatEffects(1).setOwner(a).setLifetime(lifetime);
      game.constraintStat.constraintBuilder().setJointType({ joint }).setTargets(a, {});

      Solver solver{ .game{ game }, .a{ a }, .b{}, .localCenterToPinA{}, .localCenterToPinB{} };
      std::vector<SolveTimepoint> history = trySolve(solver, 5, expectAnd(expectTargetLinearVelocity(joint.linearTarget), expectOneSidedOrientation(joint.angularTarget)));

      assertSolved(history);
    }

    TEST_METHOD(StatOneSidedMotorJointZ) {
      Game game;
      const ElementRef a = game.scene->objectA;
      constexpr size_t lifetime = 2000;
      Constraints::MotorJoint joint{ .linearTargetZ{ 0.5f }, .zForce{ 1.0f } };
      joint.flags.set(gnx::enumCast(Constraints::MotorJoint::Flags::CanPull));

      game.constraintStat.createStatEffects(1).setOwner(a).setLifetime(lifetime);
      game.constraintStat.constraintBuilder().setJointType({ joint }).setTargets(a, {});

      Solver solver{ .game{ game }, .a{ a }, .b{}, .localCenterToPinA{}, .localCenterToPinB{} };
      std::vector<SolveTimepoint> history = trySolve(solver, 10, expectTargetZVelocity(joint.linearTargetZ));

      assertSolved(history);
    }

    TEST_METHOD(StatOneSidedMotorJointWorldTargetRotationPI) {
      Game game;
      const ElementRef a = game.scene->objectA;
      constexpr size_t lifetime = 2000;
      Constraints::MotorJoint joint{ .linearTarget{ 1, 2 }, .angularTarget{ 3.141f }, .linearForce{ 1.0f }, .angularForce{ 0.1f } };
      joint.flags.set(gnx::enumCast(Constraints::MotorJoint::Flags::WorldSpaceLinear));
      joint.flags.set(gnx::enumCast(Constraints::MotorJoint::Flags::AngularOrientationTarget));
      joint.flags.set(gnx::enumCast(Constraints::MotorJoint::Flags::CanPull));

      game.constraintStat.createStatEffects(1).setOwner(a).setLifetime(lifetime);
      game.constraintStat.constraintBuilder().setJointType({ joint }).setTargets(a, {});

      Solver solver{ .game{ game }, .a{ a }, .b{}, .localCenterToPinA{}, .localCenterToPinB{} };
      std::vector<SolveTimepoint> history = trySolve(solver, 10, expectAnd(expectTargetLinearVelocity(joint.linearTarget), expectOneSidedOrientation(joint.angularTarget)));

      assertSolved(history);
    }

    static TableID getAutoManagedConstraintTable(Game& game) {
      auto ids = game->builder().queryTables<Constraints::AutoManageJointTag, Tags::DynamicPhysicsObjectsWithMotorTag>();
      Assert::IsTrue(ids.size());
      return ids[0];
    }

    static ElementRef createAutoManagedBody(Game& game) {
      auto task = game->builder();
      TableID table = getAutoManagedConstraintTable(game);
      NotifyingTableModifier mod{ task, table };
      mod.initTask(game.args);
      const ElementRef* result = mod.addElements(1);
      const size_t i = mod.toIndex(*result);
      CachedRow<Transform::WorldTransformRow> transforms;
      if(task.getResolver<>()->tryGetOrSwapAllRows(table, transforms)) {
        transforms->at(i).setPos(glm::vec2{ 10, 10 });
      }
      return *result;
    }

    TEST_METHOD(AutoManagedOneSidedMotorJoint) {
      Game game;
      const TableID table = getAutoManagedConstraintTable(game);
      ElementRef a = createAutoManagedBody(game);
      auto task = game->builder();
      Constraints::Rows rows = Constraints::Definition::resolve(task, table, 0);
      Constraints::Builder builder{ rows };
      Assert::IsTrue(rows.joint && rows.joint->size() == 1);
      const size_t i = 0;

      Constraints::MotorJoint joint{ .linearTarget{ 2, 1 }, .angularTarget{ 0 }, .linearForce{ 1.0f }, .angularForce{ 0 } };
      builder.select({ i, i + 1 }).setJointType({ joint });

      Solver solver{ .game{ game }, .a{ a }, .b{}, .localCenterToPinA{}, .localCenterToPinB{} };
      std::vector<SolveTimepoint> history = trySolve(solver, 5, expectTargetLinearVelocity(joint.linearTarget));

      assertSolved(history);

      //Remove and run long enough for gc to run and see if it crashes
      game->builder().getModifierForTable(table)->resize(0);
      for(size_t u = 0; u < 300; ++u) {
        game->update();
      }
    }

    TEST_METHOD(AutoManagedOneSidedMotorJointTargetToZero) {
      Game game;
      const TableID table = getAutoManagedConstraintTable(game);
      ElementRef a = createAutoManagedBody(game);
      auto task = game->builder();
      Constraints::Rows rows = Constraints::Definition::resolve(task, table, 0);
      Constraints::Builder builder{ rows };
      const size_t i = 0;

      Constraints::MotorJoint joint{ .linearTarget{ 2, 1 }, .angularTarget{ 1 }, .linearForce{ 1.0f }, .angularForce{ 1 } };
      joint.flags.set(gnx::enumCast(Constraints::MotorJoint::Flags::CanPull));
      builder.select({ i, i + 1 }).setJointType({ joint });

      Solver solver{ .game{ game }, .a{ a }, .b{}, .localCenterToPinA{}, .localCenterToPinB{} };
      trySolve(solver, 2, nullptr);
      joint.angularTarget = 0;
      builder.setJointType({ joint });

      std::vector<SolveTimepoint> history = trySolve(solver, 5, expectTargetAngularVelocity(joint.angularTarget));

      assertSolved(history);
    }

    static void assertUnconstrainted(Game& game, const std::vector<ElementRef>& bodies) {
      pt::VelocityResolver velocity{ game->builder(), pt::MutableVelocities::create(PhysicsSimulation::getPhysicsAliases()) };
      for(const ElementRef& b : bodies) {
        auto v = velocity.resolve(b);
        //Make sure the resolver works
        Assert::IsFalse(v.lessThan(0.01f));
        //Zero out velocities. Since constraints are gone nothing else should move them
        velocity.writeBack({}, b);
      }

      game->update();

      for(const ElementRef& b : bodies) {
        auto v = velocity.resolve(b);
        Assert::IsTrue(v.lessThan(0.01f));
      }
    }

    TEST_METHOD(StatJointExpiration) {
      Game game;
      const ElementRef a = game.scene->objectA;
      const ElementRef b = game.scene->objectB;
      constexpr size_t lifetime = 5;
      Constraints::WeldJoint joint{ .localCenterToPinA{ 1.0f, 0.0f }, .localCenterToPinB{ -1.0f, 0.0f } };

      game.constraintStat.createStatEffects(1).setOwner(a).setLifetime(lifetime);
      game.constraintStat.constraintBuilder().setJointType({ joint }).setTargets(a, b);

      Solver solver{ game, a, b, joint.localCenterToPinA, joint.localCenterToPinB };
      //This currently requires waiting for the Constraint GC to kick in. If faster removal is required it could be configurable on the AutoManageJoint row
      trySolve(solver, lifetime + 203, expectWithinDistanceAndAngle(0.0f, 0.0f));

      assertUnconstrainted(game, { a, b });
    }

    TEST_METHOD(StatJointDestroyBodyA) {
      Game game;
      const ElementRef a = game.scene->objectA;
      const ElementRef b = game.scene->objectB;
      Constraints::WeldJoint joint{ .localCenterToPinA{ 1.0f, 0.0f }, .localCenterToPinB{ -1.0f, 0.0f } };

      game.constraintStat.createStatEffects(1).setOwner(a).setLifetime(1000);
      game.constraintStat.constraintBuilder().setJointType({ joint }).setTargets(a, b);

      Solver solver{ game, a, b, joint.localCenterToPinA, joint.localCenterToPinB };
      trySolve(solver, 5, expectWithinDistanceAndAngle(0.0f, 0.0f));

      game.markForDestruction(a);

      //Process removal
      game->update();

      assertUnconstrainted(game, { b });
    }

    TEST_METHOD(StatJointDestroyBodyB) {
      Game game;
      const ElementRef a = game.scene->objectA;
      const ElementRef b = game.scene->objectB;
      Constraints::WeldJoint joint{ .localCenterToPinA{ 1.0f, 0.0f }, .localCenterToPinB{ -1.0f, 0.0f } };

      game.constraintStat.createStatEffects(1).setOwner(a).setLifetime(1000);
      game.constraintStat.constraintBuilder().setJointType({ joint }).setTargets(a, b);

      Solver solver{ game, a, b, joint.localCenterToPinA, joint.localCenterToPinB };
      trySolve(solver, 5, expectWithinDistanceAndAngle(0.0f, 0.0f));

      game.markForDestruction(b);

      //Process removal
      game->update();

      assertUnconstrainted(game, { a });
    }

    //TODO: configuring custom constraint using a scene task
  };
}