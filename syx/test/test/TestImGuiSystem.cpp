#include "Precompile.h"
#include "test/TestImGuiSystem.h"

#include "ecs/component/ImGuiContextComponent.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace TestImGui {
  using namespace Engine;
  using ImGuiView = View<Write<ImGuiContextComponent>>;

  void tickInit(SystemContext<EntityFactory>&) {
    ImGuiIO& io = ImGui::GetIO();
    //Prevent saving anything to disk during tests
    io.IniFilename = nullptr;
    //Arbitrary values, just needed to be able to accept commands
    io.DisplaySize = ImVec2(100.f, 100.f);
    io.DisplayFramebufferScale = ImVec2(1.f, 1.f);
    io.DeltaTime = 0.01f;
    unsigned char* pixels;
    int width, height;
    //Don't care about font but need to call this so it doesn't assert about being uninitialized
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    ImGui::NewFrame();
  }

  void tickUpdate(SystemContext<ImGuiView>& context) {
    if(context.get<ImGuiView>().tryGetFirst()) {
      ImGui::EndFrame();
      ImGui::NewFrame();
    }
  }
}

std::shared_ptr<Engine::System> TestImGuiSystem::init() {
  return ecx::makeSystem("TestImGuiInit", &TestImGui::tickInit, IMGUI_THREAD);
}

std::shared_ptr<Engine::System> TestImGuiSystem::update() {
  return ecx::makeSystem("TestImGuiUpdate", &TestImGui::tickUpdate, IMGUI_THREAD);
}
