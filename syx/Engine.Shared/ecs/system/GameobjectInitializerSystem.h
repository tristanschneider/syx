#pragma once

#include "ecs/ECS.h"

struct GameobjectInitializerSystem {
  static std::shared_ptr<Engine::System> create();
  static std::shared_ptr<Engine::System> createDestructionProcessor();
};