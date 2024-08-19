#include "Precompile.h"
#include "CppUnitTest.h"

#include "AppBuilder.h"
#include "TestGame.h"
#include "SceneNavigator.h"
#include "RowTags.h"
#include "TableAdapters.h"

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
          Tags::PosXRow, Tags::PosYRow,
          const StableIDRow>();
        Assert::IsFalse(q.matchingTableIDs.empty());
        auto modifier = task.getModifierForTable(q.matchingTableIDs[0]);
        task.setCallback([=](AppTaskArgs&) mutable {
          const size_t base = modifier->addElements(2);
          auto&& [_, x, y, ref] = q.get(0);
          const size_t a = base;
          const size_t b = base + 1;
          objectA = ref->at(a);
          objectB = ref->at(b);

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
      TestGame* operator->() {
        return &game;
      }

      TestGame& operator*() {
        return game;
      }

      std::unique_ptr<Scene> temp = std::make_unique<Scene>();
      Scene* scene = temp.get();
      TestGame game{ std::move(temp) };
    };

    TEST_METHOD(BasicStatCreateDestroy) {
      Game game;


    }
  };
}