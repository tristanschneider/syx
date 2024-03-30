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

  void updateFragmentGoals(IAppBuilder& builder);

  //Process before table service runs
  void preProcessEvents(IAppBuilder& builder);

  void _migrateCompletedFragments(IAppBuilder& builder);
}