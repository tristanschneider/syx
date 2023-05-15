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
  TaskRange updateFragmentGoals(GameDB db);
  //TODO: task range and make parallel
  void updateFragmentForces(GameDB db);

  void _setupScene(GameDB db, SceneArgs args);
  void _migrateCompletedFragments(GameDB db);
}