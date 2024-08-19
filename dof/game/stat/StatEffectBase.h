#pragma once

#include "curve/CurveDefinition.h"
#include "generics/IndexRange.h"
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
  constexpr size_t INSTANT = 0;
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
    std::vector<ElementRef> toRemove, newlyAdded;
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
  struct Owner : Row<ElementRef>{};
  struct Lifetime : Row<size_t>{};
  struct Global : SharedRow<Globals>{};
  struct Continuations : Row<Continuation>{};
  //Optional for effects that have a target in addition to an owner
  struct Target : Row<ElementRef>{};

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

  std::shared_ptr<TaskNode> tickLifetime(Lifetime& lifetime, const StableIDRow& stableIDs, std::vector<ElementRef>& toRemove, size_t removeOnTick);
  void tickLifetime(IAppBuilder* builder, const TableID& table, size_t removeOnTick);

  void processRemovals(IAppBuilder& builder, const TableID& table);

  //Resolves the stable id then calls the continuation if there is any. Resolution is a bit redundant, removal needs to do it again because it's moving them around as part of removal
  void processCompletionContinuation(IAppBuilder& builder, const TableID& table);

  void solveCurves(IAppBuilder& builder, const TableID& table, const CurveAlias& alias);

  class BuilderBase {
  public:
    template<class TableT>
    struct Args {
      TableT& table;
      const TableID& tableID;
      StableElementMappings& mappings;
    };

    template<class TableT>
    BuilderBase(Args<TableT> args)
      : owner{ &std::get<StatEffect::Owner>(args.table.mRows) }
      , lifetime{ &std::get<StatEffect::Lifetime>(args.table.mRows) }
      , global{ &std::get<StatEffect::Global>(args.table.mRows) }
      , continuations{ &std::get<StatEffect::Continuations>(args.table.mRows) }
      , modifier{ StableTableModifierInstance::get(args.table, args.tableID, args.mappings) }
      , target{ TableOperations::tryGetRow<StatEffect::Target>(args.table) }
      , curveInput{ TableOperations::tryGetRow<StatEffect::CurveInput<>>(args.table) }
      , curveOutput{ TableOperations::tryGetRow<StatEffect::CurveOutput<>>(args.table) }
      , curveDefinition{ TableOperations::tryGetRow<StatEffect::CurveDef<>>(args.table) } {
    }

    //Each set of commands must begin with this, creates the range of effects in this table
    BuilderBase& createStatEffects(size_t count);
    BuilderBase& setLifetime(size_t value);
    BuilderBase& setOwner(const ElementRef& stableID);

  protected:
    gnx::IndexRange currentEffects;

  private:
    StatEffect::Owner* owner{};
    StatEffect::Lifetime* lifetime{};
    StatEffect::Global* global{};
    StatEffect::Continuations* continuations{};
    StableTableModifierInstance modifier;

    //Optionals
    StatEffect::Target* target{};
    Row<float>* curveInput{};
    Row<float>* curveOutput{};
    Row<CurveDefinition*>* curveDefinition{};
  };
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