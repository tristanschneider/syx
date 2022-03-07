#pragma once

#include "ecs/ECS.h"

//Populates the DeltaTimeComponent with the real delta time
//For deterministic simulation it likely makes more sense to rely on the register app phase's tick rate
//and use constant tick intervals in the logic
struct DeltaTimeSystem {
  //Creates the DeltaTimeComponent
  static std::shared_ptr<Engine::System> init();
  //Uses real time to update the value on the DeltaTimeComponent
  static std::shared_ptr<Engine::System> update();
};