#pragma once

namespace SceneNavigator {
  struct IScene;
}

namespace Scenes {
  std::unique_ptr<SceneNavigator::IScene> createFragmentScene();
}