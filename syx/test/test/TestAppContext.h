#pragma once

#include "AppRegistration.h"
#include "ecs/ECS.h"

//Creates an Engine::AppContext with default app registration applied
struct TestAppContext {
  TestAppContext() {
    Registration::createDefaultApp()->registerAppContext(mContext);
  };

  Engine::AppContext* operator->() {
    return &mContext;
  }

  Engine::AppContext& operator*() {
    return mContext;
  }

  std::shared_ptr<Engine::Scheduler> mScheduler = std::make_shared<Engine::Scheduler>(ecx::SchedulerConfig{});
  Engine::AppContext mContext = Engine::AppContext(mScheduler);
};