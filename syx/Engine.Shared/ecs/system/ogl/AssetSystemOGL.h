#pragma once

#include "ecs/ECS.h"

struct AssetSystemOGL {
  static std::shared_ptr<Engine::System> uploadTextures();
};