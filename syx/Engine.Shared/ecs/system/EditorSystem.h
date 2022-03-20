#pragma once

#include "ecs/ECS.h"

struct EditorSystem {
  static const char* WINDOW_NAME;
  static const char* NEW_OBJECT_LABEL;
  static const char* DELETE_OBJECT_LABEL;
  static const char* OBJECT_LIST_NAME;

  static std::shared_ptr<Engine::System> init();

  static std::shared_ptr<Engine::System> sceneBrowser();

  static std::shared_ptr<Engine::System> createUriListener();
};