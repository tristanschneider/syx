#include "Precompile.h"
#include "DebugModule.h"

#include "imgui.h"
#include "AppBuilder.h"
#include "Renderer.h"
#include "ImguiModule.h"
#include "TableAdapters.h"
#include <time/TimeModule.h>

namespace DebugModule {
  void debugWindow(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setPinning(AppTaskPinning::MainThread{}).setName("debug text");
    Config::GameConfig* config = TableAdapters::getGameConfigMutable(task);
    const bool* enabled = ImguiModule::queryIsEnabled(task);
    Time::TimeConfig* time = TimeModule::getTimeConfigMutable(task);
    task.setCallback([enabled, config, time](AppTaskArgs&) {
      if(!*enabled) {
        return;
      }
      ImGui::Begin("Debug");
      ImGui::Checkbox("Draw Fragment AI", &config->fragment.drawAI);
      bool paused = time->simRate == 0;
      if(ImGui::Checkbox("Pause", &paused)) {
        time->simRate = paused ? math::Ratio32{ 0 } : math::Ratio32{ 1, Time::TimeConfig::DEFAULT_RATE };
      }
      ImGui::End();
    });
    builder.submitTask(std::move(task));
  }

  void update(IAppBuilder& builder) {
    debugWindow(builder);
  }
}
