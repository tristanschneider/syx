#pragma once

#include "ecs/ECS.h"

struct ImGuiSystems {
  static std::shared_ptr<Engine::System> init();
  static std::shared_ptr<Engine::System> render();
  static std::shared_ptr<Engine::System> updateInput();
};