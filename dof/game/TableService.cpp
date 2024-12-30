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
      auto resolver = ids->getRefResolver();
      for(const DBEvents::MoveCommand& cmd : events.toBeMovedElements) {
        const ElementRef* source = std::get_if<ElementRef>(&cmd.source);
        const TableID* destination = std::get_if<TableID>(&cmd.destination);
        if(cmd.isCreate()) {
          //Nothing currently, may want to support this in the future
        }
        else if(cmd.isDestroy()) {
          assert(source);
          //Remove each element referenced by removal events
          if(std::optional<UnpackedDatabaseElementID> resolved = resolver.tryUnpack(*source)) {
            if(RuntimeTable* table = db.tryGet(TableID{ *resolved })) {
              table->swapRemove(resolved->getElementIndex());
            }
          }
        }
        else if(cmd.isMove()) {
          assert(source && destination);
          //Move all elements to the desired location
          if(std::optional<UnpackedDatabaseElementID> resolved = resolver.tryUnpack(*source)) {
            const UnpackedDatabaseElementID& rawSrc = *resolved;
            if(rawSrc.getTableIndex() == destination->getTableIndex()) {
              continue;
            }
            RuntimeTable* fromTable = db.tryGet(TableID{ rawSrc });
            RuntimeTable* toTable = db.tryGet(*destination);
            if(fromTable && toTable) {
              RuntimeTable::migrate(rawSrc.getElementIndex(), *fromTable, *toTable, 1);
            }
          }
        }
      }
    });

    builder.submitTask(std::move(task));
  }
}