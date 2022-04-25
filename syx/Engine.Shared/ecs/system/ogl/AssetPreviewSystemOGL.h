#pragma once

#include "ecs/ECS.h"

struct AssetPreviewSystemOGL {
  static std::shared_ptr<Engine::System> previewTexture();
};