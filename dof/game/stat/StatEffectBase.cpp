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
          --remaining;
        }
        else {
          //Let the removal processing resolve he unstable id, set invalid here
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

  std::shared_ptr<TaskNode> resolveOwners(GameDB db, Owner& owners, StableInfo stable) {
    return TaskNode::create([&owners, db, stable](...) {
      StableElementMappings& mappings = *stable.mappings;
      for(size_t i = 0; i < owners.size(); ++i) {
        StableElementID& toResolve = owners.at(i);
        //Normal stats could rely on resolving upfront before processing stat but since lambda has
        //access to the entire database it could cause table migrations
        toResolve = StableOperations::tryResolveStableID(toResolve, db.db, mappings).value_or(StableElementID::invalid());
      }
    });
  }
}