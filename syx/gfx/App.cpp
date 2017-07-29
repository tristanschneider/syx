#include "Precompile.h"
#include "App.h"
#include "GraphicsSystem.h"
#include "KeyboardInput.h"

App::App() {
  mSystems.resize(static_cast<int>(SystemId::Count), nullptr);
  _registerSystem(mGraphics, SystemId::Graphics);
  _registerSystem(mKeyboardInput, SystemId::KeyboardInput);
}

App::~App() {
}

void App::init() {
  for(System* system : mSystems) {
    system->init();
  }
}

void App::update(float dt) {
  for(System* system : mSystems) {
    system->update(dt);
  }
}

void App::uninit() {
  for(System* system : mSystems) {
    system->uninit();
  }
}