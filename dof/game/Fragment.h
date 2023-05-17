#pragma once

struct TaskRange;
struct GameDB;
struct SceneArgs;

namespace Fragment {
  struct SceneArgs {
    size_t mFragmentRows{};
    size_t mFragmentColumns{};
  };

  void setupScene(GameDB db);
  //Read GPos, FragmentGoal, FragmentGoalFoundRow, StableIDRow
  //Write FragmentGoalFoundRow
  //Modify thread locals
  TaskRange updateFragmentGoals(GameDB db);

  void _setupScene(GameDB db, SceneArgs args);
  void _migrateCompletedFragments(GameDB db, size_t thread);
}