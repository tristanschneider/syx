#pragma once

class IAppBuilder;
struct TaskRange;
struct GameDB;
struct SceneArgs;

struct FragmentAdapter;

namespace Fragment {
  void updateFragmentGoals(IAppBuilder& builder);

  //Process before table service runs
  void preProcessEvents(IAppBuilder& builder);

  void _migrateCompletedFragments(IAppBuilder& builder);
}