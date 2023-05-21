#include "Precompile.h"
#include "GameModule.h"
#include "Simulation.h"

#include "curve/CurveSolver.h"
#include "imgui.h"
#include "ImguiExt.h"
#include "TableAdapters.h"
#include "File.h"
#include "config/ConfigIO.h"

namespace Toolbox {
  void update(GameDB db) {
    GameConfig* config = TableAdapters::getConfig(db).game;
    FileSystem* fileSystem = TableAdapters::getGlobals(db).fileSystem;
    if(ImGui::Button("Save Configuration")) {
      std::string buffer = ConfigIO::serializeJSON(ConfigConvert::toConfig(*config));
      if(File::writeEntireFile(*fileSystem, Simulation::getConfigName(), buffer)) {
        printf("Configuration saved\n");
      }
      else {
        printf("Failed to save configuration\n");
      }
    }
  }
}

void GameModule::update(GameDB db) {
  GameConfig& config = *TableAdapters::getConfig(db).game;
  ImGui::Begin("Game");

  Toolbox::update(db);

  {
    static ImguiExt::CurveSliders sliders;
    sliders.label = "Linear Player Acceleration";
    sliders.durationRange = { 0.0f, 1.0f };
    sliders.offsetRange = { -0.1f, 0.24f };
    sliders.scaleRange = { 0.0f, 0.15f };
    ImguiExt::curve(config.player.linearMoveCurve, sliders);
  }
  {
    static ImguiExt::CurveSliders sliders;
    sliders.label = "Linear Player Deceleration";
    sliders.durationRange = { 0.0f, 1.0f };
    sliders.offsetRange = { -0.1f, 0.24f };
    sliders.scaleRange = { 0.0f, 0.15f };
    ImguiExt::curve(config.player.linearStoppingCurve, sliders);
  }
  {
    static ImguiExt::CurveSliders sliders;
    sliders.label = "Angular Player Acceleration";
    sliders.durationRange = { 0.0f, 1.0f };
    sliders.offsetRange = { -0.1f, 0.5f };
    sliders.scaleRange = { 0.0f, 1.0f };
    ImguiExt::curve(config.player.angularMoveCurve, sliders);
  }

  ImguiExt::inputSizeT("Explode Lifetime", &config.ability.explodeLifetime);
  ImGui::SliderFloat("Explode Strength", &config.ability.explodeStrength, 0.0f, 1.0f);
  ImGui::SliderFloat("Camera Zoom Speed", &config.camera.cameraZoomSpeed, 0.0f, 1.0f);
  ImGui::SliderFloat("Boundary Spring Force", &config.world.boundarySpringConstant, 0.0f, 1.0f);
  ImGui::SliderFloat("Fragment Goal Distance", &config.fragment.fragmentGoalDistance, 0.0f, 5.0f);
  ImguiExt::inputSizeT("Fragment Rows", &config.fragment.fragmentRows);
  ImguiExt::inputSizeT("Fragment Columns", &config.fragment.fragmentColumns);
  ImGui::End();
}
