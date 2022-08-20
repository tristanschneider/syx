#pragma once

#include "AppRegistration.h"
#include "ecs/component/FileSystemComponent.h"
#include "ecs/ECS.h"
#include "test/TestFileSystem.h"
#include "test/TestImGuiSystem.h"

//Creates an Engine::AppContext with default app registration applied
struct TestAppContext {
  struct TestEntityRegistry : public Engine::EntityRegistry {
    using Base = Engine::EntityRegistry;

    auto createEntity() {
      return Base::createEntity(*getDefaultEntityGenerator());
    }

    void destroyEntity(Engine::Entity entity) {
      Base::destroyEntity(entity, *getDefaultEntityGenerator());
    }

    template<class... Args>
    auto createEntityWithComponents() {
      return Base::createEntityWithComponents<Args...>(*getDefaultEntityGenerator());
    }

    template<class... Args>
    auto createAndGetEntityWithComponents() {
      return Base::createAndGetEntityWithComponents<Args...>(*getDefaultEntityGenerator());
    }
  };

  TestAppContext() {
    Registration::createDefaultApp()->registerAppContext(mContext);

    //Hack to get this to work, same as MockEditorApp. Should have better handling for when imgui implementation isn't registered
    Engine::AppContext::PhaseContainer initializers = mContext.getInitializers();
    initializers.mSystems.push_back(TestImGuiSystem::init());

    Engine::AppContext::PhaseContainer input = mContext.getUpdatePhase(Engine::AppPhase::Input);
    input.mSystems.insert(input.mSystems.begin(), TestImGuiSystem::update());

    mContext.registerInitializer(std::move(initializers.mSystems));
    mContext.registerUpdatePhase(Engine::AppPhase::Input, std::move(input.mSystems), input.mTargetFPS);

    mContext.buildExecutionGraph();

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
  TestEntityRegistry mRegistry;
  FileSystem::TestFileSystem* mFS = nullptr;
};