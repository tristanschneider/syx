#pragma once

#include "ecs/ECS.h"

struct SetWorkingDirectoryComponent;
struct UriActivationComponent;

struct ProjectLocatorSystem {
  static std::unique_ptr<Engine::System> init();

  static std::optional<SetWorkingDirectoryComponent> tryParseSetWorkingDirectory(const UriActivationComponent& uri);

  //Sets the project root based on uri activation
  static std::unique_ptr<Engine::System> createUriListener();
};