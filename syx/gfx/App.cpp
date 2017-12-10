#include "Precompile.h"
#include "App.h"
#include "system/GraphicsSystem.h"
#include "threading/WorkerPool.h"
#include "threading/Task.h"
#include "EditorNavigator.h"
#include "Space.h"
#include "ImGuiImpl.h"

App::App() {
  System::Registry::getSystems(*this, mSystems);
  mDefaultSpace = std::make_unique<Space>(*this);
  mWorkerPool = std::make_unique<WorkerPool>(3);
}

App::~App() {
}

void App::init() {
  for(auto& system : mSystems) {
    if(system)
      system->init();
  }

  GraphicsSystem& gfx = *getSystem<GraphicsSystem>();
  mAssets["car"] = gfx.addModel("models/car.obj");
  mAssets["bowser"] = gfx.addModel("models/bowserlow.obj");
  mAssets["maze"] = gfx.addTexture("textures/test.bmp");
  mAssets["cube"] = gfx.addModel("models/cube.obj");

  mDefaultSpace->init();
}

void App::update(float dt) {

  auto frameTask = std::make_shared<Task>();

  mDefaultSpace->update(dt);
  for(auto& system : mSystems) {
    if(system)
      system->update(dt, *mWorkerPool, frameTask);
  }
  mWorkerPool->queueTask(frameTask);

  }

  std::weak_ptr<Task> weakFrame = frameTask;
  frameTask = nullptr;
  mWorkerPool->sync(weakFrame);
}

void App::uninit() {
  mDefaultSpace->uninit();
  for(auto& system : mSystems) {
    if(system)
      system->uninit();
  }
}

Space& App::getDefaultSpace() {
  return *mDefaultSpace;
}

IWorkerPool& App::getWorkerPool() {
  return *mWorkerPool;
}
