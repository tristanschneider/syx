#pragma once

#include "AppContext.h"
#include "ECS.h"
#include "EngineScheduler.h"

namespace Engine {
  enum class AppPhase : uint8_t {
    Input,
    Simulation,
    Physics,
    Graphics,
    Cleanup,
  };

  inline static auto getTimeImpl() {
    return std::chrono::steady_clock::now();
  }

  using AppContext = ecx::AppContext<Scheduler, ecx::Timer<20, &getTimeImpl>, AppPhase, Entity>;
  using SystemList = std::vector<std::shared_ptr<System>>;
}