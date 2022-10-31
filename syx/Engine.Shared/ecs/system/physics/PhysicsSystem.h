#pragma once

#include "ecs/ECS.h"

struct PhysicsSystems {
  static std::vector<std::shared_ptr<Engine::System>> createDefault();
};