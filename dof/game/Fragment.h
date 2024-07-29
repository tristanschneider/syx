#pragma once

#include "StableElementID.h"

struct AppTaskArgs;
class ElementRef;
class IAppBuilder;
struct TaskRange;
struct GameDB;
struct SceneArgs;
class RuntimeDatabaseTaskBuilder;
struct FragmentAdapter;

namespace Fragment {
  using FragmentCooldownT = uint8_t;
  struct FragmentGoalCooldownDefinition {
    //Counts up every tick by dt. When it passes timeToTick then all FragmentGoalCooldowns are decremented
    float currentTime{};
    //Set at configuration time, determines how long each fragment cooldown tick is worth
    float timeToTick{ 1.0f };
  };

  //When nonzero, prevents finding goal and ticks down at the rate defined by FragmentGoalCooldownDefinition
  struct FragmentGoalCooldownRow : Row<FragmentCooldownT> {};
  struct FragmentGoalCooldownDefinitionRow : SharedRow<FragmentGoalCooldownDefinition> {};

  class IFragmentMigrator {
  public:
    virtual ~IFragmentMigrator() = default;
    virtual void moveActiveToComplete(const ElementRef& activeFragment, AppTaskArgs& args) = 0;
    virtual void moveCompleteToActive(const ElementRef& completeFragment, AppTaskArgs& args) = 0;
  };

  struct FragmentTables {
    FragmentTables(RuntimeDatabaseTaskBuilder& task);
    TableID completeTable{};
    TableID activeTable{};
  };

  void updateFragmentGoals(IAppBuilder& builder);

  //Process before table service runs
  void preProcessEvents(IAppBuilder& builder);
  void postProcessEvents(IAppBuilder& builder);

  void _migrateCompletedFragments(IAppBuilder& builder);

  std::shared_ptr<IFragmentMigrator> createFragmentMigrator(RuntimeDatabaseTaskBuilder& task);
}