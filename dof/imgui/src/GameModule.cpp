#include "Precompile.h"
#include "GameModule.h"
#include "Simulation.h"

#include "imgui.h"
#include "ImguiExt.h"
#include "TableAdapters.h"

void GameModule::update(GameDB db) {
  GameConfig& config = *TableAdapters::getConfig(db).game;
  ImGui::Begin("Game");
  ImguiExt::inputSizeT("Explode Lifetime", &config.ability.explodeLifetime);
  ImGui::SliderFloat("Explode Strength", &config.ability.explodeStrength, 0.0f, 1.0f);
  ImGui::SliderFloat("Camera Zoom Speed", &config.camera.cameraZoomSpeed, 0.0f, 1.0f);
  ImGui::SliderFloat("Boundary Spring Force", &config.world.boundarySpringConstant, 0.0f, 1.0f);
  ImGui::SliderFloat("Fragment Goal Distance", &config.fragment.fragmentGoalDistance, 0.0f, 5.0f);
  ImguiExt::inputSizeT("Fragment Rows", &config.fragment.fragmentRows);
  ImguiExt::inputSizeT("Fragment Columns", &config.fragment.fragmentColumns);
  ImGui::End();
}
