#pragma once
#include "ecs/ECS.h"

struct GlobalCommandBufferSystem {
  //Creates a single global command buffer component
  static std::shared_ptr<Engine::System> init();
  //Process all commands in the above command buffer
  static std::shared_ptr<Engine::System> processCommands();
};