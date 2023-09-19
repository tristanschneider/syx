#pragma once

class IAppBuilder;
struct TaskRange;
struct GameDB;
struct SceneArgs;

struct FragmentAdapter;

namespace Fragment {
  struct SceneArgs {
    size_t mFragmentRows{};
    size_t mFragmentColumns{};
  };

  void applyDamage(FragmentAdapter& fragments, size_t i, float damage);

  void setupScene(IAppBuilder& builder);
  //Read GPos, FragmentGoal, FragmentGoalFoundRow, StableIDRow
  //Write FragmentGoalFoundRow
  //Modify thread locals
  TaskRange updateFragmentGoals(GameDB db);

  //Process before table service runs
  TaskRange processEvents(GameDB db);

  void _migrateCompletedFragments(GameDB db, size_t thread);
}