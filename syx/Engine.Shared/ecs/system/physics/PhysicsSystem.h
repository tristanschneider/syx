#pragma once

#include "ecs/ECS.h"

struct PhysicsSystems {
  static std::shared_ptr<Engine::System> createInit();
  static std::vector<std::shared_ptr<Engine::System>> createDefault();
};