#include "Precompile.h"
#include "App.h"
#include "system/GraphicsSystem.h"
#include "threading/WorkerPool.h"
#include "threading/Task.h"
#include "EditorNavigator.h"
#include "Space.h"
#include "ImGuiImpl.h"

App::App() {
  mDefaultSpace = std::make_unique<Space>(*this);
  mWorkerPool = std::make_unique<WorkerPool>(3);
  System::Registry::getSystems(*this, mSystems);
}

App::~App() {
}

void App::init() {
  for(auto& system : mSystems) {
    if(system)
      system->init();
  }

  GraphicsSystem& gfx = *getSystem<GraphicsSystem>();
  mAssets["maze"] = gfx.addTexture("textures/test.bmp");

  mDefaultSpace->init();
}

#include "imgui/imgui.h"

void App::update(float dt) {

  auto frameTask = std::make_shared<Task>();

  mDefaultSpace->update(dt);
  for(auto& system : mSystems) {
    if(system)
      system->update(dt, *mWorkerPool, frameTask);
  }
  mWorkerPool->queueTask(frameTask);

  if(ImGuiImpl::enabled()) {
    static float f = 0.0f;
    ImGui::Text("Hello, world!");
    ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
    static Syx::Vec3 clear_color(0.0f);
    static bool show_test_window = true;
    static bool show_another_window = true;
    ImGui::ColorEdit3("clear color", (float*)&clear_color);
    if(ImGui::Button("Test Window")) show_test_window ^= 1;
    if(ImGui::Button("Another Window")) show_another_window ^= 1;
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    const int buffSize = 100;
    static char buff[buffSize] = { 0 };
    ImGui::InputText("Text In", buff, buffSize);
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
