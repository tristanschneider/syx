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
    });
    return TaskBuilder::addEndSync(root);
  }
}