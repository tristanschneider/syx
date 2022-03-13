#pragma once

#include "ecs/ECS.h"

//Responsible for openGL specific graphics functionality
//For generally applicable logic, use GraphicsSystemBase
struct GraphicsSystemOGL {
  static std::shared_ptr<Engine::System> init();
  static std::shared_ptr<Engine::System> onWindowResized();
  static std::shared_ptr<Engine::System> render();
  static std::shared_ptr<Engine::System> swapBuffers();
};