#pragma once

#include "ecs/ECS.h"

struct GraphicsSystemBase {
  static std::shared_ptr<Engine::System> init();

  static std::shared_ptr<Engine::System> screenSizeListener();
};