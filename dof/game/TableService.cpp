#include "Precompile.h"

#include "DBEvents.h"
#include "Simulation.h"
#include "TableAdapters.h"

namespace TableService {
  TaskRange processEvents(GameDB db) {
    auto root = TaskNode::create([db](...) {
      const DBEvents& events = Events::getPublishedEvents(db);
      StableElementMappings& mappings = TableAdapters::getStableMappings(db);

      for(const DBEvents::MoveCommand& cmd : events.toBeMovedElements) {
        if(cmd.isCreate()) {
          //Nothing currently, may want to support this in the future
        }
        else if(cmd.isDestroy()) {
          //Remove each element referenced by removal events
          if(std::optional<StableElementID> resolved = StableOperations::tryResolveStableID(cmd.source, db.db, mappings)) {
            const GameDatabase::ElementID raw = resolved->toPacked<GameDatabase>();
            db.db.visitOneByIndex(raw, [&]([[maybe_unused]] auto& table) {
              using T = std::decay_t<decltype(table)>;
              if constexpr(TableOperations::isStableTable<T>) {
                TableOperations::stableSwapRemove(table, raw, mappings);
              }
            });
          }
        }
        else if(cmd.isMove()) {
          //Move all elements to the desired location
          if(std::optional<StableElementID> resolved = StableOperations::tryResolveStableID(cmd.source, db.db, mappings)) {
            const GameDatabase::ElementID rawSrc = resolved->toPacked<GameDatabase>();
            const GameDatabase::ElementID rawDst = cmd.destination.toPacked<GameDatabase>();
            if(rawSrc.getTableIndex() == rawDst.getTableIndex()) {
              continue;
            }
            db.db.visitOneByIndex(rawSrc, [&](auto& sourceTable) {
              db.db.visitOneByIndex(rawDst, [&](auto& dstTable) {
                using ST = std::decay_t<decltype(sourceTable)>;
                using DT = std::decay_t<decltype(dstTable)>;
                if constexpr(TableOperations::isStableTable<ST> && TableOperations::isStableTable<DT>) {
                  TableOperations::stableMigrateOne(sourceTable, dstTable, rawSrc, rawDst, mappings);
                }
              });
            });
          }
        }
      }
    });
    return TaskBuilder::addEndSync(root);
  }
}