#pragma once

#include "ecs/ECS.h"
#include "JobInfo.h"
#include "Scheduler.h"

namespace Engine {
  using Scheduler = ecx::DefaultSchedulerT<Entity>;
  using JobInfo = ecx::JobInfo<Entity>;
};