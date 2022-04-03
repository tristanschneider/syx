#pragma once

#include "ecs/ECS.h"

struct RawInputSystem {
  //Creates an entity with RawInputBufferComponent and RawInputComponent
  static std::shared_ptr<Engine::System> init();
  //Updates RawInputComponent state with the new events from the RawInputBufferComponent
  //OS specific systems are expected to have populated the buffer with events
  static std::shared_ptr<Engine::System> update();
};