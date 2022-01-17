#pragma once

#include "ecs/ECS.h"

struct ProjectLocatorSystem {
  static std::unique_ptr<Engine::System> init();

  //Sets the project root based on uri activation
  static std::unique_ptr<Engine::System> createUriListener();
};