#pragma once

#include "curve/CurveDefinition.h"
#include "generics/IndexRange.h"
#include "SceneNavigator.h"
#include "Scheduler.h"
#include "StableElementID.h"
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
    std::vector<CurveAlias> curves;
  };

  struct ConfigRow : SharedRow<Config> {};

  //The gameobject this affects
  struct Owner : Row<ElementRef>{};
  struct Lifetime : Row<size_t>{};
  //Optional for effects that have a target in addition to an owner
  struct Target : Row<ElementRef>{};

  struct StatEffectTagRow : TagRow{};

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
    BuilderBase(RuntimeTable& t, RuntimeDatabase& localDatabase)
      : owner{ t.tryGet<StatEffect::Owner>() }
      , lifetime{ t.tryGet<StatEffect::Lifetime>() }
      , localDB{ localDatabase }
      , table{ t }
      , target{ t.tryGet<StatEffect::Target>() }
      , curveInput{ t.tryGet<StatEffect::CurveInput<>>() }
      , curveOutput{ t.tryGet<StatEffect::CurveOutput<>>() }
      , curveDefinition{ t.tryGet<StatEffect::CurveDef<>>() } {
    }

    //Each set of commands must begin with this, creates the range of effects in this table
    BuilderBase& createStatEffects(size_t count);
    BuilderBase& setLifetime(size_t value);
    BuilderBase& setOwner(const ElementRef& stableID);

  protected:
    gnx::IndexRange currentEffects;
    RuntimeTable& table;

  private:
    StatEffect::Owner* owner{};
    StatEffect::Lifetime* lifetime{};
    RuntimeDatabase& localDB;

    //Optionals
    StatEffect::Target* target{};
    Row<float>* curveInput{};
    Row<float>* curveOutput{};
    Row<CurveDefinition*>* curveDefinition{};
  };
}

template<class... Rows>
struct StatEffectBase : Table<
  StatEffect::StatEffectTagRow,
  SceneNavigator::IsClearedWithSceneTag,
  StatEffect::ConfigRow,
  StatEffect::Owner,
  StatEffect::Lifetime,
  StableIDRow,
  Rows...
> {};