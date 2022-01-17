#pragma once

#include "ecs/ECS.h"

struct EditorSystem {
  static std::shared_ptr<Engine::System> init();

  static std::shared_ptr<Engine::System> createUriListener();
};