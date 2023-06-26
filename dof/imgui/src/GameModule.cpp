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
    Config::GameConfig* config = TableAdapters::getConfig(db).game;
    FileSystem* fileSystem = TableAdapters::getGlobals(db).fileSystem;
    if(ImGui::Button("Save Configuration")) {
      std::string buffer = ConfigIO::serializeJSON(*config);
      if(File::writeEntireFile(*fileSystem, Simulation::getConfigName(), buffer)) {
        printf("Configuration saved\n");
      }
      else {
        printf("Failed to save configuration\n");
      }
    }
  }
}

namespace CameraModule {
  void update(GameDB db) {
    Config::GameConfig& config = *TableAdapters::getConfig(db).game;
    ImGui::Begin("Camera");
    {
      static ImguiExt::CurveSliders sliders;
      sliders.label = "Follow";
      sliders.durationRange = { 0.0f, 1.0f };
      sliders.offsetRange = { 0.0f, 1.24f };
      sliders.scaleRange = { 0.0f, 5.0f };
      ImguiExt::curve(config.camera.followCurve, sliders);
      //TODO: hook this up
      //ImGui::DragFloat("Zoom", &config.camera.zoom, 1.0f, 0.01f);
    }
    ImGui::End();
  }
}

void GameModule::update(GameDB db) {
  CameraModule::update(db);

  Config::GameConfig& config = *TableAdapters::getConfig(db).game;
  ImGui::Begin("Game");

  Toolbox::update(db);
  {
    static ImguiExt::CurveSliders sliders;
    sliders.label = "Linear Speed";
    sliders.durationRange = { 0.0f, 1.0f };
    sliders.offsetRange = { 0.0f, 1.24f };
    sliders.scaleRange = { 0.0f, 5.0f };
    ImguiExt::curve(config.player.linearSpeedCurve, sliders);
  }
  {
    static ImguiExt::CurveSliders sliders;
    sliders.label = "Linear Force";
    sliders.durationRange = { 0.0f, 1.0f };
    sliders.offsetRange = { 0.0f, 0.5f };
    sliders.scaleRange = { 0.0f, 1.0f };
    ImguiExt::curve(config.player.linearForceCurve, sliders);
  }
  {
    static ImguiExt::CurveSliders sliders;
    sliders.label = "Linear Stopping Speed";
    sliders.durationRange = { 0.0f, 1.0f };
    sliders.offsetRange = { 0.0f, 1.24f };
    sliders.scaleRange = { 0.0f, 5.0f };
    ImguiExt::curve(config.player.linearStoppingSpeedCurve, sliders);
  }
  {
    static ImguiExt::CurveSliders sliders;
    sliders.label = "Linear Stopping Force";
    sliders.durationRange = { 0.0f, 1.0f };
    sliders.offsetRange = { 0.0f, 0.0f };
    sliders.scaleRange = { 0.0f, 0.25f };
    ImguiExt::curve(config.player.linearStoppingForceCurve, sliders);
  }

  {
    static ImguiExt::CurveSliders sliders;
    sliders.label = "Angular Speed";
    sliders.durationRange = { 0.0f, 1.0f };
    sliders.offsetRange = { 0.0f, 1.24f };
    sliders.scaleRange = { 0.0f, 5.0f };
    ImguiExt::curve(config.player.angularSpeedCurve, sliders);
  }
  {
    static ImguiExt::CurveSliders sliders;
    sliders.label = "Angular Force";
    sliders.durationRange = { 0.0f, 1.0f };
    sliders.offsetRange = { 0.0f, 0.5f };
    sliders.scaleRange = { 0.0f, 1.0f };
    ImguiExt::curve(config.player.angularForceCurve, sliders);
  }
  {
    static ImguiExt::CurveSliders sliders;
    sliders.label = "Angular Stopping Speed";
    sliders.durationRange = { 0.0f, 1.0f };
    sliders.offsetRange = { 0.0f, 0.5f };
    sliders.scaleRange = { 0.0f, 1.0f };
    ImguiExt::curve(config.player.angularStoppingSpeedCurve, sliders);
  }
  {
    static ImguiExt::CurveSliders sliders;
    sliders.label = "Angular Stopping Force";
    sliders.durationRange = { 0.0f, 1.0f };
    sliders.offsetRange = { 0.0f, 0.5f };
    sliders.scaleRange = { 0.0f, 1.0f };
    ImguiExt::curve(config.player.angularStoppingForceCurve, sliders);
  }
  ImGui::Checkbox("Draw Move", &config.player.drawMove);


  ImguiExt::inputSizeT("Explode Lifetime", &config.ability.explodeLifetime);
  ImGui::SliderFloat("Explode Strength", &config.ability.explodeStrength, 0.0f, 1.0f);
  ImGui::SliderFloat("Camera Zoom Speed", &config.camera.cameraZoomSpeed, 0.0f, 1.0f);
  ImGui::SliderFloat("Boundary Spring Force", &config.world.boundarySpringConstant, 0.0f, 1.0f);
  ImGui::SliderFloat("Fragment Goal Distance", &config.fragment.fragmentGoalDistance, 0.0f, 5.0f);
  ImguiExt::inputSizeT("Fragment Rows", &config.fragment.fragmentRows);
  ImguiExt::inputSizeT("Fragment Columns", &config.fragment.fragmentColumns);
  ImGui::End();
}
