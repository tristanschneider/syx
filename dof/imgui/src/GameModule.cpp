#include "Precompile.h"
#include "GameModule.h"
#include "Simulation.h"

#include "imgui.h"
#include "ImguiExt.h"
#include "TableAdapters.h"

void GameModule::update(GameDB db) {
  GameConfig& config = *TableAdapters::getConfig(db).game;
  ImGui::Begin("Game");
  ImGui::SliderFloat("Player Speed", &config.playerSpeed, 0.0f, 1.0f);
  ImGui::SliderFloat("Player Stopping Force", &config.playerMaxStoppingForce, 0.0f, 1.0f);
  ImguiExt::inputSizeT("Explode Lifetime", &config.explodeLifetime);
  ImGui::SliderFloat("Explode Strength", &config.explodeStrength, 0.0f, 1.0f);
  ImGui::SliderFloat("Camera Zoom Speed", &config.cameraZoomSpeed, 0.0f, 1.0f);
  ImGui::SliderFloat("Boundary Spring Force", &config.boundarySpringConstant, 0.0f, 1.0f);
  ImGui::SliderFloat("Fragment Goal Distance", &config.fragmentGoalDistance, 0.0f, 5.0f);
  ImguiExt::inputSizeT("Fragment Rows", &config.fragmentRows);
  ImguiExt::inputSizeT("Fragment Columns", &config.fragmentColumns);
  ImGui::End();
}
