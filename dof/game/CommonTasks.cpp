#include "Precompile.h"
#include "CommonTasks.h"

#include "TableAdapters.h"
#include "ThreadLocals.h"

namespace CommonTasks {
  void migrateThreadLocalDBsToMain(IAppBuilder& builder) {
    auto task = builder.createTask();
    RuntimeDatabase& main = task.getDatabase();
    ThreadLocals& tls = TableAdapters::getThreadLocals(task);

    task.setCallback([&main, &tls](AppTaskArgs&) {
      for(size_t i = 0; i < tls.getThreadCount(); ++i) {
        //TODO: dirty flags to avoid traversing all tables every time
        RuntimeDatabase& threadDB = *tls.get(i).statEffects;
        for(size_t t = 0; t < threadDB.size(); ++t) {
          RuntimeTable& threadTable = threadDB[t];
          const size_t elements = threadTable.size();
          if(!elements) {
            continue;
          }
          RuntimeTable& mainTable = main[t];
          assert(mainTable.getID() == threadTable.getID());

          //TODO: what to do about requested moves?
          //TODO: stat specifics?
          //Migrate everything from the local table to the main table
          RuntimeTable::migrate(0, threadTable, mainTable, elements);
        }
      }
    });

    builder.submitTask(std::move(task.setName("Migrate DBs")));
  }
}