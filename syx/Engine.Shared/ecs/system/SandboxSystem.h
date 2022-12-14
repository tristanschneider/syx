#pragma once

#include "ecs/ECS.h"

//System to hack around with hardcoded logic for manual testing
struct SandboxSystem {
  static std::shared_ptr<Engine::System> create();
};