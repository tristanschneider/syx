#include "Precompile.h"
#include "stat/StatEffectBase.h"

#include "Simulation.h"
#include "TableAdapters.h"

namespace StatEffect {
  std::shared_ptr<TaskNode> tickLifetime(Lifetime& lifetime, const StableIDRow& stableIDs, std::vector<StableElementID>& toRemove) {
    return TaskNode::create([&lifetime, &stableIDs, &toRemove](...) {
      for(size_t i = 0; i < lifetime.size(); ++i) {
        size_t& remaining = lifetime.at(i);
        if(remaining) {
          if(remaining != INFINITE) {
            --remaining;
          }
        }
        else {
          //Let the removal processing resolve the unstable id, set invalid here
          toRemove.push_back(StableElementID::fromStableRow(i, stableIDs));
        }
      }
    });
  }

  std::shared_ptr<TaskNode> processRemovals(void* table, TableOperations::StableSwapRemover remove, std::vector<StableElementID>& toRemove, StableInfo stable) {
    return TaskNode::create([table, remove, &toRemove, stable](...) {
      for(StableElementID id : toRemove) {
        if(auto resolved = StableOperations::tryResolveStableIDWithinTable(id, stable)) {
          remove(table, resolved->toUnpacked(stable.description), *stable.mappings);
        }
      }
      toRemove.clear();
    });
  }

  std::shared_ptr<TaskNode> processCompletionContinuation(GameDB db, Continuations& continuations, std::vector<StableElementID>& toRemove, StableInfo stable) {
    return TaskNode::create([db, stable, &toRemove, &continuations](enki::TaskSetPartition, uint32_t thread) mutable {
      for(StableElementID& id : toRemove) {
        if(auto resolved = StableOperations::tryResolveStableIDWithinTable(id, stable)) {
          //Store the resolved id for processRemovals
          id = *resolved;

          //Find and call the continuation, if any
          const UnpackedDatabaseElementID unpacked = id.toUnpacked(stable.description);
          Continuation c = std::move(continuations.at(unpacked.getElementIndex()));
          if(!c.onComplete.empty()) {
            Continuation::Callback fn{ std::move(c.onComplete.front()) };
            c.onComplete.pop_front();

            Continuation::Args args {
              db,
              unpacked,
              thread,
              std::move(c)
            };

            fn(args);
          }
        }
      }
    });
  }

  std::shared_ptr<TaskNode> resolveOwners(GameDB db, Row<StableElementID>& owners, StableInfo stable) {
    return TaskNode::create([&owners, db, stable](...) {
      StableElementMappings& mappings = *stable.mappings;
      for(size_t i = 0; i < owners.size(); ++i) {
        StableElementID& toResolve = owners.at(i);
        toResolve = StableOperations::tryResolveStableID(toResolve, db.db, mappings).value_or(StableElementID::invalid());
      }
    });
  }
}