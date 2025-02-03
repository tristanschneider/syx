#include "Precompile.h"
#include "CommonTasks.h"

#include "Events.h"
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
          Events::EventsRow* events = mainTable.tryGet<Events::EventsRow>();
          if(!stable || !events) {
            continue;
          }

          //Emit creation events for all the newly migrated elements
          for(; m < mainTable.size(); ++m) {
            events->getOrAdd(m).setCreate();
          }
        }

        threadDB.clearDirtyTables();
      }
    });

    builder.submitTask(std::move(task.setName("Migrate DBs")));
  }
}