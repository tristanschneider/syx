#include <time/TimeModule.h>
#include <TestGame.h>
#include <CppUnitTest.h>
#include <Game.h>
#include <RuntimeDatabase.h>
#include <AppBuilder.h>
#include <TLSTaskImpl.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Test {
  TEST_CLASS(TimeModuleTest) {
    struct TickInfo {
      std::vector<Time::TimeSlice> simulation, physics, rendering;
    };
    struct TickInfoRow : SharedRow<TickInfo> {};

    struct TestTask {
      void init(RuntimeDatabaseTaskBuilder& task) {
        info = task.query<TickInfoRow>().tryGetSingletonElement();
        time = TimeModule::getTime(task);
      }

      void execute() {
        info->simulation.push_back(time->simulation);
        info->physics.push_back(time->physics);
        info->rendering.push_back(time->rendering);
      }

      TickInfo* info{};
      const Time::TimeTransform* time{};
    };

    struct TestModule : IAppModule {
      void createDatabase(RuntimeDatabaseArgs& db) final {
        StorageTableBuilder table;
        table.addRows<TickInfoRow>();
        std::move(table).finalize(db);
      }

      void update(IAppBuilder& builder) {
        builder.submitTask(TLSTask::create<TestTask>("test"));
      }
    };

    struct TimeGame : TestGame {
      static GameConstructArgs create() {
        auto args = GameDefaults::createDefaultGameArgs();
        args.modules.push_back(std::make_unique<TestModule>());
      }

      TickInfo* info{ test->query<TickInfoRow>().tryGetSingletonElement() };
    };

    TEST_METHOD(asdf) {
      TestGame game;
      game.update();
      game;

    }
  };
}