#include "Precompile.h"
#include "App.h"
#include "GraphicsSystem.h"
#include "KeyboardInput.h"
#include "EditorNavigator.h"

App::App() {
  mSystems.resize(static_cast<size_t>(SystemId::Count));
  _registerSystem<GraphicsSystem>();
  _registerSystem<KeyboardInput>();
  _registerSystem<EditorNavigator>();
}

App::~App() {
}

void App::init() {
  for(auto& system : mSystems) {
    if(system)
      system->init();
  }
}

void App::update(float dt) {
  for(auto& system : mSystems) {
    if(system)
      system->update(dt);
  }
}

void App::uninit() {
  for(auto& system : mSystems) {
    if(system)
      system->uninit();
  }
}