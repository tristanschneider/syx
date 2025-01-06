#include "Precompile.h"
#include "CommonTasks.h"

#include "TableAdapters.h"
#include "ThreadLocals.h"

namespace CommonTasks {
  void migrateThreadLocalDBsToMain(IAppBuilder& builder) {
    auto task = builder.createTask();
    RuntimeDatabase& main = task.getDatabase();
    ThreadLocals& tls = TableAdapters::getThreadLocals(task);

    task.setCallback([&main, &tls](AppTaskArgs& args) {
      for(size_t i = 0; i < tls.getThreadCount(); ++i) {
        RuntimeDatabase& threadDB = *tls.get(i).statEffects;
        for(auto [t, v] : threadDB.getDirtyTables()) {
          RuntimeTable& threadTable = threadDB[t];
          const size_t elements = threadTable.size();
          if(!elements) {
            continue;
          }
          RuntimeTable& mainTable = main[t];
          assert(mainTable.getType() == threadTable.getType());

          //Migrate everything from the local table to the main table
          size_t m = RuntimeTable::migrate(0, threadTable, mainTable, elements);
          StableIDRow* stable = mainTable.tryGet<StableIDRow>();
          if(!stable) {
            continue;
          }

          //Emit creation events for all the newly migrated elements
          IDBEvents& events = *args.getEvents();
          for(; m < mainTable.size(); ++m) {
            DBEvents::MoveCommand cmd;
            cmd.destination = stable->at(m);
            events.emit(std::move(cmd));
          }
        }

        threadDB.clearDirtyTables();
      }
    });

    builder.submitTask(std::move(task.setName("Migrate DBs")));
  }
}