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

    TEST_METHOD(StatPinJoin) {
      Game game;
      const ElementRef a = game.scene->objectA;
      const ElementRef b = game.scene->objectB;
      constexpr size_t lifetime = 2000;
      Constraints::PinJoint joint{ .localCenterToPinA{ 1.0f, 0.0f }, .localCenterToPinB{ -1.0f, 0.0f } };
      pt::TransformResolver transform{ game->builder(), PhysicsSimulation::getPhysicsAliases() };

      game.constraintStat.createStatEffects(1).setOwner(a).setLifetime(lifetime);
      game.constraintStat.constraintBuilder().setJointType({ joint }).setTargets(a, b);
      std::vector<SolveTimepoint> history;

      for(size_t i = 0; i < 1000; ++i) {
        game->update();
        const pt::Transform ta = transform.resolve(a);
        const pt::Transform tb = transform.resolve(b);
        SolveTimepoint solve{
          .ta = ta,
          .tb = tb,
          .worldA = ta.transformPoint(joint.localCenterToPinA),
          .worldB = tb.transformPoint(joint.localCenterToPinB),
        };
        solve.error = glm::distance(solve.worldA, solve.worldB);
        history.push_back(solve);
      }

      Assert::AreEqual(0.0f, history.back().error, 0.01f);
      //If something weird happened both transforms could be zero which should still fail the test
      Assert::AreNotEqual(0.0f, glm::length(history.back().worldA), 0.01f);
    }

    //TODO: one sided constraint
    //TODO: configuring custom constraint using a scene task
  };
}