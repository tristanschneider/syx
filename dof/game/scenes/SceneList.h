#pragma once

#include "Table.h"

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
  };
  struct ListNavigator {
    const Scenes* scenes{};
    std::shared_ptr<SceneNavigator::INavigator> navigator;
  };
  struct ScenesRow : SharedRow<Scenes> {};

  ListNavigator createNavigator(RuntimeDatabaseTaskBuilder& task);
  const Scenes* get(RuntimeDatabaseTaskBuilder& task);
  void registerScenes(IAppBuilder& builder);
};