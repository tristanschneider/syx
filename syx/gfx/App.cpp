#include "Precompile.h"
#include "App.h"
#include "GraphicsSystem.h"

App::App()
  : mGraphics(std::make_unique<GraphicsSystem>()) {
}

App::~App() {
}


void App::init() {
  mGraphics->init();
}

void App::update(float dt) {
  mGraphics->update(dt);
}

void App::uninit() {
  mGraphics->uninit();
}
