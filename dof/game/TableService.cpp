#include "Precompile.h"

#include "DBEvents.h"
#include "Simulation.h"
#include "TableAdapters.h"
#include "AppBuilder.h"

namespace TableService {
  void processEvents(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("Process events");
    RuntimeDatabase& db = task.getDatabase();
    const DBEvents& events = Events::getPublishedEvents(task);
    auto ids = task.getIDResolver();

    task.setCallback([&db, &events, ids](AppTaskArgs&) mutable {
      for(const DBEvents::MoveCommand& cmd : events.toBeMovedElements) {
        if(cmd.isCreate()) {
          //Nothing currently, may want to support this in the future
        }
        else if(cmd.isDestroy()) {
          //Remove each element referenced by removal events
          if(std::optional<ResolvedIDs> resolved = ids->tryResolveAndUnpack(cmd.source)) {
            if(RuntimeTable* table = db.tryGet(resolved->unpacked)) {
              assert(table->stableModifier);
              table->stableModifier.modifier.swapRemove(table->stableModifier.table, resolved->unpacked, *table->stableModifier.stableMappings);
            }
          }
        }
        else if(cmd.isMove()) {
          //Move all elements to the desired location
          if(std::optional<ResolvedIDs> resolved = ids->tryResolveAndUnpack(cmd.source)) {
            const UnpackedDatabaseElementID& rawSrc = resolved->unpacked;
            const UnpackedDatabaseElementID rawDst = ids->uncheckedUnpack(cmd.destination);
            if(rawSrc.getTableIndex() == rawDst.getTableIndex()) {
              continue;
            }
            RuntimeTable* fromTable = db.tryGet(rawSrc);
            RuntimeTable* toTable = db.tryGet(rawDst);
            if(fromTable && toTable) {
              RuntimeTable::migrateOne(rawSrc.getElementIndex(), *fromTable, *toTable);
            }
          }
        }
      }
    });

    builder.submitTask(std::move(task));
  }
}