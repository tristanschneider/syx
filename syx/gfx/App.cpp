#include "Precompile.h"
#include "App.h"
#include "systems/GraphicsSystem.h"
#include "systems/KeyboardInput.h"
#include "EditorNavigator.h"
#include "Space.h"

App::App() {
  mSystems.resize(static_cast<size_t>(SystemId::Count));
  _registerSystem<GraphicsSystem>();
  _registerSystem<KeyboardInput>();
  _registerSystem<EditorNavigator>();
  mDefaultSpace = std::make_unique<Space>();
}

App::~App() {
}

void App::init() {
  mDefaultSpace->init();
  for(auto& system : mSystems) {
    if(system)
      system->init();
  }
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
