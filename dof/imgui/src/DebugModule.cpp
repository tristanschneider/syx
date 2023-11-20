#include "Precompile.h"
#include "DebugModule.h"

#include "imgui.h"
#include "AppBuilder.h"
#include "Renderer.h"
#include "ImguiModule.h"
#include "RendererTableAdapters.h"
#include "DebugLinePassTable.h"
#include "TableAdapters.h"

namespace DebugModule {
  constexpr auto FULL_SCREEN_INVISIBLE = ImGuiWindowFlags_NoNav |
    ImGuiWindowFlags_NoDecoration |
    ImGuiWindowFlags_NoInputs |
    ImGuiWindowFlags_NoBackground;

  //Imgui is currently the easiest way to render text so it's processed here
  //For real text I'll need either a UI solution or some font rendering
  void processDebugText(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setPinning(AppTaskPinning::MainThread{}).setName("debug text");
    const bool* enabled = ImguiModule::queryIsEnabled(task);
    auto global = RendererTableAdapters::getGlobals(task);
    auto requests = task.query<const DebugLinePassTable::Texts>();
    task.setCallback([enabled, global, requests](AppTaskArgs&) mutable {
      if(!*enabled) {
        return;
      }
      //Fill the screen
      const auto& io = ImGui::GetIO();
      ImGui::SetNextWindowSize(io.DisplaySize);
      ImGui::SetNextWindowPos({ 0, 0 });
      ImGui::Begin("debug text", nullptr, FULL_SCREEN_INVISIBLE);
      //Switch from center origin to upper left origin
      constexpr glm::vec2 TRANSLATE{ 1.0f, -1.0f };
      //Scale down by half from [-1,1] to [-0.5,0.5] then up by screen size
      const glm::vec2 SCALE{ 0.5f*io.DisplaySize.x, -0.5f*io.DisplaySize.y };
      //Translate then scale
      glm::mat4 screen(SCALE.x, 0, 0, TRANSLATE.x*SCALE.x,
        0, SCALE.y, 0, TRANSLATE.y*SCALE.y,
        0, 0, 1, 0,
        0, 0, 0, 1);
      //Constructor is column major, I want row major
      screen = glm::transpose(screen);

      for(const RendererCamera& camera : global.state->mCameras) {
        //Transform from world to view to imgui screen space
        glm::mat4 transform = screen * camera.worldToView;
        for(size_t t = 0; t < requests.size(); ++t) {
          const auto& [req] = requests.get(t);
          for(const DebugLinePassTable::Text& text : req->mElements) {
            glm::vec4 point{ text.pos.x, text.pos.y, 0, 1.0f };
            point = transform * point;
            ImGui::SetCursorPosX(point.x);
            ImGui::SetCursorPosY(point.y);
            ImGui::Text(text.text.c_str());
          }
        }
      }

      ImGui::End();
    });
    builder.submitTask(std::move(task));
  }

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
    processDebugText(builder);
    debugWindow(builder);
  }
}