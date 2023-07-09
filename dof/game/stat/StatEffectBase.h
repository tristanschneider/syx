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

  //This allows chaining together multiple effects. When an effect completes this will be called
  //which allows creating a follow-up effect. Since the given effect is being destroyed ownership
  //of the continuation needs to be transferred to the next one if it is desired to preserve the chain
  //New effects should be added through thread locals
  struct Continuation {
    struct Args {
      //The entire db is exposed yet the intended use would just be to look up any additional information
      //on the table of the element being removed, and to get the thread locals
      GameDB& db;
      //Id of the stat that is about to be removed
      const UnpackedDatabaseElementID& id;
      size_t thread{};
      Continuation&& continuation;
    };
    using Callback = std::function<void(Args&)>;
    std::deque<Callback> onComplete;
  };

  //The gameobject this affects
  struct Owner : Row<StableElementID>{};
  struct Lifetime : Row<size_t>{};
  struct Global : SharedRow<Globals>{};
  struct Continuations : Row<Continuation>{};
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

  //Resolves the stable id then calls the continuation if there is any. Resolution is a bit redundant, removal needs to do it again because it's moving them around as part of removal
  std::shared_ptr<TaskNode> processCompletionContinuation(GameDB db, Continuations& continuations, std::vector<StableElementID>& toRemove, StableInfo stable);

  std::shared_ptr<TaskNode> resolveOwners(GameDB db, Row<StableElementID>& owners, StableInfo stable);
}

template<class... Rows>
struct StatEffectBase : Table<
  StatEffect::Owner,
  StatEffect::Lifetime,
  StatEffect::Global,
  StatEffect::Continuations,
  StableIDRow,
  Rows...
> {};