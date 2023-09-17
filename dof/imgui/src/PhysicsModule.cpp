#include "Precompile.h"
#include "PhysicsModule.h"

#include "config/Config.h"
#include "Simulation.h"
#include "TableAdapters.h"
#include "imgui.h"
#include "ImguiExt.h"
#include "AppBuilder.h"
#include "ImguiModule.h"

void PhysicsModule::update(IAppBuilder& builder) {
  auto task = builder.createTask();
  task.setName("Imgui Physics").setPinning(AppTaskPinning::MainThread{});
  Config::PhysicsConfig* config = task.query<SharedRow<Config::PhysicsConfig>>().tryGetSingletonElement();
  const bool* enabled = ImguiModule::queryIsEnabled(task);
  assert(config);
  task.setCallback([config, enabled](AppTaskArgs&) mutable {
    if(!*enabled) {
      return;
    }
    ImGui::Begin("Physics");
    bool force = config->mForcedTargetWidth.has_value();
    if(ImGui::Checkbox("Force SIMD Target Width", &force)) {
      config->mForcedTargetWidth = size_t(1);
    }
    if(config->mForcedTargetWidth) {
      ImguiExt::inputSizeT("Forced Target Width", &*config->mForcedTargetWidth);
    }
    ImGui::SliderFloat("Linear Drag", &config->linearDragMultiplier, 0.5f, 1.0f);
    ImGui::SliderFloat("Angular Drag", &config->angularDragMultiplier, 0.5f, 1.0f);
    ImGui::SliderFloat("Friction Coefficient", &config->frictionCoeff, 0.0f, 1.0f);
    ImGui::InputInt("Solve Iterations", &config->solveIterations);
    ImGui::Checkbox("Draw Collision Pairs", &config->drawCollisionPairs);
    ImGui::Checkbox("Draw Contacts", &config->drawContacts);
    ImGui::Checkbox("Draw Broadphase", &config->broadphase.draw);
    ImGui::End();
  });
}
