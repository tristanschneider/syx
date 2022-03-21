#pragma once

#include "ecs/ECS.h"

//ImGui implementation with just enough for tests without the dependency on rendering and input
struct TestImGuiSystem {
  static std::shared_ptr<Engine::System> init();
  static std::shared_ptr<Engine::System> update();
};