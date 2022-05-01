#pragma once

#include "ecs/ECS.h"

struct AssetSystemOGL {
  static std::shared_ptr<Engine::System> uploadTextures();
  static std::shared_ptr<Engine::System> uploadModels();
  static std::shared_ptr<Engine::System> compileShaders();
};