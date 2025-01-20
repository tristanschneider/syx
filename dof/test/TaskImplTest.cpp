#include "Precompile.h"
#include "CppUnitTest.h"

#include "AppBuilder.h"
#include "TLSTaskImpl.h"
#include "IGame.h"
#include "Game.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Test {
  TEST_CLASS(TaskImplTest) {
    TEST_METHOD(Basic) {
      struct TLSGroup {
        TLSGroup(RuntimeDatabaseTaskBuilder& task)
          : table{ task.queryTables<Row<int>>()[0] }
        {
          values.push_back(5);
        }

        std::vector<int> values;
        TableID table;
      };
      struct TLSLocals {
        TLSLocals(AppTaskArgs& args)
          : table{ args.getLocalDB().query<Row<int>>()[0] }
        {
        }

        TableID table;
      };
      struct TestTLSTask {
        TestTLSTask(RuntimeDatabaseTaskBuilder& task)
          : table{ task.queryTables<Row<int>>()[0] }
          , modifier{ task.getModifierForTable(table) }
        {
        }

        void execute(TLSGroup& group, TLSLocals& locals, AppTaskArgs&) {
          Assert::AreEqual(5, group.values[0]);
          Assert::IsTrue(table == group.table);
          Assert::AreEqual(table.getTableIndex(), locals.table.getTableIndex());
          Assert::AreNotEqual(table.getDatabaseIndex(), locals.table.getDatabaseIndex());
          modifier->resize(1);
        }

        TableID table;
        std::shared_ptr<ITableModifier> modifier;
      };
      struct BasicTask {
        BasicTask(RuntimeDatabaseTaskBuilder& task)
          : row{ task.query<Row<int>>().tryGet<0>(0) }
        {
        }

        void execute(AppTaskArgs&) {
          Assert::AreEqual(size_t(1), row->size());
          row->at(0) = 99;
        }

        Row<int>* row{};
      };
      struct Module : IAppModule {
        void createDatabase(RuntimeDatabaseArgs& args) {
          DBReflect::addDatabase<Database<Table<Row<int>>>>(args);
        }

        void update(IAppBuilder& builder) {
          builder.submitTask(TLSTask::create<TestTLSTask, TLSGroup, TLSLocals>("name"));
          builder.submitTask(TLSTask::create<BasicTask>("basic"));
        }
      };
      Game::GameArgs args = GameDefaults::createDefaultGameArgs();
      args.modules.push_back(std::make_unique<Module>());
      std::unique_ptr<IGame> game = Game::createGame(std::move(args));
      game->init();

      game->updateSimulation();

      auto q = game->getDatabase().getRuntime().query<Row<int>>();
      Assert::AreEqual(size_t(1), q.get<0>(0).size());
      Assert::AreEqual(99, q.get<0>(0).at(0));
    }
  };
}