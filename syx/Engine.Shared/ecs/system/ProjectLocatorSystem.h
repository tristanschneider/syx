#pragma once

#include "ecs/ECS.h"

struct ProjectLocatorSystem {
  //Sets the project root based on uri activation
  static std::unique_ptr<Engine::System> createUriListener();
};