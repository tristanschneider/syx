#include "Precompile.h"
#include "PhysicsModule.h"

#include "PhysicsConfig.h"
#include "Simulation.h"
#include "TableAdapters.h"
#include "imgui.h"
#include "ImguiExt.h"

void PhysicsModule::update(GameDB db) {
  PhysicsConfig& config = *TableAdapters::getConfig(db).physics;

  ImGui::Begin("Physics");
  bool force = config.mForcedTargetWidth.has_value();
  if(ImGui::Checkbox("Force SIMD Target Width", &force)) {
    config.mForcedTargetWidth = size_t(1);
  }
  if(config.mForcedTargetWidth) {
    ImguiExt::inputSizeT("Forced Target Width", &*config.mForcedTargetWidth);
  }
  ImGui::SliderFloat("Linear Drag", &config.linearDragMultiplier, 0.5f, 1.0f);
  ImGui::SliderFloat("Angular Drag", &config.angularDragMultiplier, 0.5f, 1.0f);
  ImGui::SliderFloat("Friction Coefficient", &config.frictionCoeff, 0.0f, 1.0f);
  ImGui::InputInt("Solve Iterations", &config.solveIterations);
  ImGui::Checkbox("Draw Collision Pairs", &config.drawCollisionPairs);
  ImGui::Checkbox("Draw Contacts", &config.drawContacts);
  ImGui::End();
}
