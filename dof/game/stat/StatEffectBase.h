#pragma once

#include "curve/CurveDefinition.h"
#include "Scheduler.h"
#include "StableElementID.h"
#include "TableOperations.h"
#include "RuntimeDatabase.h"

struct AppTaskArgs;
class IAppBuilder;
class RuntimeDatabase;
struct GameDB;

namespace StatEffect {
  //Lifetime of a single frame instant effect
  constexpr size_t INSTANT = 1;
  constexpr size_t INFINITE = std::numeric_limits<size_t>::max();

  struct CurveAlias {
    QueryAlias<Row<float>> curveIn;
    QueryAlias<Row<float>> curveOut;
    QueryAlias<Row<CurveDefinition*>> curveDef;
  };

  struct Config {
    size_t removeLifetime = 0;
    std::vector<CurveAlias> curves;
  };

  struct ConfigRow : SharedRow<Config> {};

  struct Globals {
    std::vector<StableElementID> toRemove;
    //This is used to associate a given central table with its thread local counterpart
    //They are assigned on construction, shouldn't change, and have no meaning other htan equality
    size_t ID;
  };

  //This allows chaining together multiple effects. When an effect completes this will be called
  //which allows creating a follow-up effect. Since the given effect is being destroyed ownership
  //of the continuation needs to be transferred to the next one if it is desired to preserve the chain
  //New effects should be added through thread locals
  struct Continuation {
    struct Args {
      //The entire db is exposed yet the intended use would just be to look up any additional information
      //on the table of the element being removed, and to get the thread locals
      RuntimeDatabase& db;
      //Id of the stat that is about to be removed
      const UnpackedDatabaseElementID& id;
      AppTaskArgs& args;
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

  std::shared_ptr<TaskNode> tickLifetime(Lifetime& lifetime, const StableIDRow& stableIDs, std::vector<StableElementID>& toRemove, size_t removeOnTick);
  void tickLifetime(IAppBuilder* builder, const UnpackedDatabaseElementID& table, size_t removeOnTick);

  void processRemovals(IAppBuilder& builder, const UnpackedDatabaseElementID& table);

  //Resolves the stable id then calls the continuation if there is any. Resolution is a bit redundant, removal needs to do it again because it's moving them around as part of removal
  void processCompletionContinuation(IAppBuilder& builder, const UnpackedDatabaseElementID& table);

  void resolveOwners(IAppBuilder& builder, const UnpackedDatabaseElementID& table);
  void resolveTargets(IAppBuilder& builder, const UnpackedDatabaseElementID& table);

  void solveCurves(IAppBuilder& builder, const UnpackedDatabaseElementID& table, const CurveAlias& alias);
}

template<class... Rows>
struct StatEffectBase : Table<
  StatEffect::ConfigRow,
  StatEffect::Owner,
  StatEffect::Lifetime,
  StatEffect::Global,
  StatEffect::Continuations,
  StableIDRow,
  Rows...
> {};