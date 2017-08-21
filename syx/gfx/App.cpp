#include "Precompile.h"
#include "App.h"
#include "system/GraphicsSystem.h"
#include "system/KeyboardInput.h"
#include "system/MessagingSystem.h"
#include "system/PhysicsSystem.h"
#include "EditorNavigator.h"
#include "Space.h"

App::App() {
  mSystems.resize(static_cast<size_t>(SystemId::Count));
  _registerSystem<MessagingSystem>();
  _registerSystem<KeyboardInput>();
  _registerSystem<EditorNavigator>();
  _registerSystem<PhysicsSystem>();
  _registerSystem<GraphicsSystem>();
  mDefaultSpace = std::make_unique<Space>(*this);
}

App::~App() {
}

void App::init() {
  for(auto& system : mSystems) {
    if(system)
      system->init();
  }

  GraphicsSystem& gfx = getSystem<GraphicsSystem>(SystemId::Graphics);
  mAssets["car"] = gfx.addModel("models/car.obj");
  mAssets["bowser"] = gfx.addModel("models/bowserlow.obj");
  mAssets["maze"] = gfx.addTexture("textures/test.bmp");
  mAssets["cube"] = gfx.addModel("models/cube.obj");

  mDefaultSpace->init();
}

void App::update(float dt) {
  mDefaultSpace->update(dt);
  for(auto& system : mSystems) {
    if(system)
      system->update(dt);
  }
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
