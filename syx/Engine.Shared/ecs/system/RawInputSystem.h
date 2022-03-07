#pragma once

#include "ecs/ECS.h"

enum class KeyState : uint8_t;
struct RawInputComponent;

struct RawInputSystem {
  static KeyState getAsciiState(const RawInputComponent& input, char c);

  //Creates an entity with RawInputBufferComponent and RawInputComponent
  static std::shared_ptr<Engine::System> init();
  //Updates RawInputComponent state with the new events from the RawInputBufferComponent
  //OS specific systems are expected to have populated the buffer with events
  static std::shared_ptr<Engine::System> update();
};