#include "Precompile.h"
#include "DebugModule.h"

#include "imgui.h"
#include "AppBuilder.h"
#include "Renderer.h"
#include "ImguiModule.h"
#include "TableAdapters.h"

namespace DebugModule {
  void debugWindow(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setPinning(AppTaskPinning::MainThread{}).setName("debug text");
    Config::GameConfig* config = TableAdapters::getGameConfigMutable(task);
    const bool* enabled = ImguiModule::queryIsEnabled(task);
    task.setCallback([enabled, config](AppTaskArgs&) {
      if(!*enabled) {
        return;
      }
      ImGui::Begin("Debug");
      ImGui::Checkbox("Draw Fragment AI", &config->fragment.drawAI);
      ImGui::End();
    });
    builder.submitTask(std::move(task));
  }

  void update(IAppBuilder& builder) {
    debugWindow(builder);
  }
}
