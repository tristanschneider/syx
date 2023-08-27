#include "Precompile.h"

#include "DBEvents.h"
#include "Simulation.h"
#include "TableAdapters.h"

namespace TableService {
  TaskRange processEvents(GameDB db) {
    auto root = TaskNode::create([db](...) {
      const DBEvents& events = Events::getPublishedEvents(db);
      StableElementMappings& mappings = TableAdapters::getStableMappings(db);

      //Remove each element referenced by removal events
      for(const StableElementID& id : events.toBeRemovedElements) {
        if(std::optional<StableElementID> resolved = StableOperations::tryResolveStableID(id, db.db, mappings)) {
          const GameDatabase::ElementID raw = resolved->toPacked<GameDatabase>();
          db.db.visitOneByIndex(raw, [&]([[maybe_unused]] auto& table) {
            using T = std::decay_t<decltype(table)>;
            if constexpr(TableOperations::isStableTable<T>) {
              TableOperations::stableSwapRemove(table, raw, mappings);
            }
          });
        }
      }

      //Move all elements to the desired location
      for(const DBEvents::MoveCommand& cmd : events.toBeMovedElements) {
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
    });
    return TaskBuilder::addEndSync(root);
  }
}