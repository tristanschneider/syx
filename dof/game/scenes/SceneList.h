#pragma once

#include "Table.h"

class IAppModule;
class IAppBuilder;
class RuntimeDatabaseTaskBuilder;

namespace SceneNavigator {
  using SceneID = size_t;
  struct INavigator;
}

namespace SceneList {
  struct Scenes {
    SceneNavigator::SceneID loading{};
    SceneNavigator::SceneID empty{};
    SceneNavigator::SceneID fragment{};
    SceneNavigator::SceneID singleStack{};
    SceneNavigator::SceneID imported{};
  };
  struct ListNavigator {
    const Scenes* scenes{};
    std::shared_ptr<SceneNavigator::INavigator> navigator;
  };
  struct ScenesRow : SharedRow<Scenes> {};

  ListNavigator createNavigator(RuntimeDatabaseTaskBuilder& task);
  const Scenes* get(RuntimeDatabaseTaskBuilder& task);

  std::unique_ptr<IAppModule> createModule();
  //Start on the a default scene by adding this module.
  //Split out from the base createModule to make it easier for tests to start with a blank slate
  std::unique_ptr<IAppModule> createStartingSceneModule();
};