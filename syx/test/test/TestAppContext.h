#pragma once

#include "AppRegistration.h"
#include "ecs/component/FileSystemComponent.h"
#include "ecs/ECS.h"
#include "test/TestFileSystem.h"

//Creates an Engine::AppContext with default app registration applied
struct TestAppContext {
  TestAppContext() {
    Registration::createDefaultApp()->registerAppContext(mContext);

    mContext.initialize(mRegistry);
    //Replace real file system with test one
    auto it = mRegistry.begin<FileSystemComponent>();
    assert(it != mRegistry.end<FileSystemComponent>());
    auto fs = std::make_unique<FileSystem::TestFileSystem>();
    mFS = fs.get();
    *it = FileSystemComponent{ std::move(fs) };
  };

  Engine::AppContext* operator->() {
    return &mContext;
  }

  Engine::AppContext& operator*() {
    return mContext;
  }

  void update() {
    mContext.addTickToAllPhases();
    mContext.update(mRegistry, size_t(1));
  }

  std::shared_ptr<Engine::Scheduler> mScheduler = std::make_shared<Engine::Scheduler>(ecx::SchedulerConfig{});
  Engine::AppContext mContext = Engine::AppContext(mScheduler);
  Engine::EntityRegistry mRegistry;
  FileSystem::TestFileSystem* mFS = nullptr;
};