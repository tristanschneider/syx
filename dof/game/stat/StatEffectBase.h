#pragma once

#include "curve/CurveDefinition.h"
#include "Scheduler.h"
#include "StableElementID.h"
#include "TableOperations.h"

struct GameDB;

namespace StatEffect {
  //Lifetime of a single frame instant effect
  constexpr size_t INSTANT = 1;
  constexpr size_t INFINITE = std::numeric_limits<size_t>::max();

  struct Globals {
    std::vector<StableElementID> toRemove;
  };

  //The gameobject this affects
  struct Owner : Row<StableElementID>{};
  struct Lifetime : Row<size_t>{};
  struct Global : SharedRow<Globals>{};
  //Optional for effects that have a target in addition to an owner
  struct Target : Row<StableElementID>{};

  //General purpose optional rows that can be in a stat table and will be filled in if provided
  //Would be more efficient to share a single curve across a table with a shared row, but also less flexible
  //The tag is used to match input, output, and definition together to solve a single curve,
  //while allowing multiple curve types in a table by providing multiple with different tags
  struct DefaultCurveTag{};

  template<class CurveTag = DefaultCurveTag>
  struct CurveInput : Row<float> {};
  template<class CurveTag = DefaultCurveTag>
  struct CurveOutput : Row<float>{};
  template<class CurveTag = DefaultCurveTag>
  struct CurveDef: Row<CurveDefinition*>{};

  std::shared_ptr<TaskNode> tickLifetime(Lifetime& lifetime, const StableIDRow& stableIDs, std::vector<StableElementID>& toRemove);

  std::shared_ptr<TaskNode> processRemovals(void* table, TableOperations::StableSwapRemover remove, std::vector<StableElementID>& toRemove, StableInfo stable);
  template<class TableT>
  std::shared_ptr<TaskNode> processRemovals(TableT& effects, StableInfo stable) {
    return processRemovals(&effects, TableOperations::getStableSwapRemove<TableT>(), std::get<Global>(effects.mRows).at().toRemove, stable);
  }

  std::shared_ptr<TaskNode> resolveOwners(GameDB db, Row<StableElementID>& owners, StableInfo stable);
}

template<class... Rows>
struct StatEffectBase : Table<
  StatEffect::Owner,
  StatEffect::Lifetime,
  StatEffect::Global,
  StableIDRow,
  Rows...
> {};