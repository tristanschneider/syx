#pragma once

#include "Scheduler.h"
#include "StableElementID.h"
#include "TableOperations.h"

struct GameDB;

namespace StatEffect {
  //Lifetime of a single frame instant effect
  constexpr size_t INSTANT = 1;

  struct Globals {
    std::vector<StableElementID> toRemove;
  };

  //The gameobject this affects
  struct Owner : Row<StableElementID>{};
  struct Lifetime : Row<size_t>{};
  struct Global : SharedRow<Globals>{};

  std::shared_ptr<TaskNode> tickLifetime(Lifetime& lifetime, const StableIDRow& stableIDs, std::vector<StableElementID>& toRemove);

  std::shared_ptr<TaskNode> processRemovals(void* table, TableOperations::StableSwapRemover remove, std::vector<StableElementID>& toRemove, StableInfo stable);
  template<class TableT>
  std::shared_ptr<TaskNode> processRemovals(TableT& effects, StableInfo stable) {
    return processRemovals(&effects, TableOperations::getStableSwapRemove<TableT>(), std::get<Global>(effects.mRows).at().toRemove, stable);
  }

  std::shared_ptr<TaskNode> resolveOwners(GameDB db, Owner& owners, StableInfo stable);
}

template<class... Rows>
struct StatEffectBase : Table<
  StatEffect::Owner,
  StatEffect::Lifetime,
  StatEffect::Global,
  StableIDRow,
  Rows...
> {};