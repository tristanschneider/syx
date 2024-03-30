#include "Precompile.h"
#include "scenes/EmptyScene.h"

#include "SceneNavigator.h"

namespace Scenes {
  struct EmptyScene : SceneNavigator::IScene {
    void init(IAppBuilder&) final {}
    void update(IAppBuilder&) final {}
    void uninit(IAppBuilder&) final {}
  };

  std::unique_ptr<SceneNavigator::IScene> createEmptyScene() {
    return std::make_unique<EmptyScene>();
  }
}