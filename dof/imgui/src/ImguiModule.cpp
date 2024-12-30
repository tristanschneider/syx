#include "Precompile.h"
#include "ImguiModule.h"

#include "imgui.h"

#include "glm/mat4x4.hpp"

#include "GameModule.h"
#include "PhysicsModule.h"
#include "GraphicsModule.h"
#include "DebugModule.h"
#include "GameInput.h"
#include "InspectorModule.h"
#include "AppBuilder.h"
#include "Database.h"
#include "Renderer.h"

#include "sokol_app.h"
#include "sokol_gfx.h"
//Impl is in SokolImpl.cpp except for imgui since that needs to come after the imgui.h include...
//Would be a bit nicer for imgui to also conditionally be in SokolImpl but it makes for a mess of dependencies
#define SOKOL_IMPL
#include "util/sokol_imgui.h"

struct ImguiEnabled : SharedRow<bool> {};

struct AllImgui {
  bool* enabled{};
};

AllImgui queryAllImgui(RuntimeDatabaseTaskBuilder& task) {
  auto q = task.query<ImguiEnabled>();
  return {
    q.tryGetSingletonElement<0>(),
  };
}

using ImguiDB = Database<
  Table<
    ImguiEnabled,
    GameInput::PlayerKeyboardInputRow,
    InspectorModule::InspectorRow
  >
>;

namespace ImguiModule {
  void updateImgui(bool& enabled) {
    if(!ImGui::GetCurrentContext()) {
      simgui_setup(simgui_desc_t{
        .depth_format = SG_PIXELFORMAT_DEPTH,
        .ini_filename = "imgui.ini"
      });
      enabled = true;
    }
    simgui_new_frame(simgui_frame_desc_t{
      .width = sapp_width(),
      .height = sapp_height(),
      .delta_time = sapp_frame_duration(),
      .dpi_scale = sapp_dpi_scale()
    });
  }

  void updateBase(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("Imgui Base").setPinning(AppTaskPinning::MainThread{});
    //Is acquiring mutable access even though it's only used const
    //Doesn't matter at the moment since the tasks are all main thread pinned anyway
    AllImgui context = queryAllImgui(task);
    Renderer::injectRenderDependency(task);
    assert(context.enabled && "Context expected to always exist in database");
    task.setCallback([context](AppTaskArgs&) mutable {
      updateImgui(*context.enabled);
    });
    builder.submitTask(std::move(task));
  }

  void updateModules(IAppBuilder& builder) {
    GameModule::update(builder);
    DebugModule::update(builder);
    GraphicsModule::update(builder);
    PhysicsModule::update(builder);
    InspectorModule::update(builder);
  }

  void updateRendering(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("Imgui Render").setPinning(AppTaskPinning::MainThread{});
    Renderer::injectRenderDependency(task);
    const bool* enabled = ImguiModule::queryIsEnabled(task);
    task.setCallback([enabled](AppTaskArgs&) mutable {
      simgui_render();
    });
    builder.submitTask(std::move(task));
  }

  void update(IAppBuilder& builder) {
    updateBase(builder);
    updateModules(builder);
    updateRendering(builder);
  }

  const bool* queryIsEnabled(RuntimeDatabaseTaskBuilder& task) {
    //Intentionally non-const to force any imgui tasks to run in sequence
    auto q = task.query<ImguiEnabled>();
    return q.tryGetSingletonElement();
  }

  void createDatabase(RuntimeDatabaseArgs& args) {
    DBReflect::addDatabase<ImguiDB>(args);
  }
}