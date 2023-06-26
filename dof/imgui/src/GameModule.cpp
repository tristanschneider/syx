#include "Precompile.h"
#include "GameModule.h"
#include "Simulation.h"

#include "curve/CurveSolver.h"
#include "imgui.h"
#include "ImguiExt.h"
#include "TableAdapters.h"
#include "File.h"
#include "config/ConfigIO.h"
#include "ability/PlayerAbility.h"
#include "Player.h"

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

namespace AbilityModule {
  template<size_t S>
  int findIndex(const std::string& toFind, const std::array<const char*, S>& values) {
    auto it = std::find(values.begin(), values.end(), toFind);
    return it != values.end() ? static_cast<int>(std::distance(values.begin(), it)) : 0;
  }

  bool edit(Ability::InstantTrigger&) {
    return false;
  }

  bool edit(Ability::ChargeTrigger& t) {
    bool changed = false;
    changed |= ImGui::SliderFloat("Min charge Percent", &t.minimumCharge, 0.0f, t.chargeCurve.params.duration.value_or(1.0f));
    static ImguiExt::CurveSliders sliders;
    sliders.label = "Charge";
    sliders.durationRange = { 0.0f, 5.0f };
    sliders.offsetRange = { 0.0f, 5.0f };
    sliders.scaleRange = { 0.0f, 5.0f };
    changed |= ImguiExt::curve(t.chargeCurve, sliders);

    return changed;
  }

  bool edit(Ability::DisabledCooldown& c) {
    return ImGui::SliderFloat("Cooldown Seconds", &c.maxTime, 0.0f, 5.0f);
  }

  bool editAbility(Config::AbilityConfigExt& ability, const char* name) {
    if(!ImGui::TreeNode(name)) {
      return false;
    }

    Ability::AbilityInput& raw = Config::getAbility(ability);
    Config::AbilityConfig cfg = ability.adapter->read();
    auto writeCfgToRaw = [&] {
      Config::AbilityConfigExt ext;
      Config::createFactory()->init(ext);
      ext.adapter->write(cfg);
      raw = Config::getAbility(ext);
    };

    int currentTrigger = findIndex(cfg.trigger.type, Ability::Strings::Trigger::ALL);
    bool changed = false;
    if(ImGui::Combo("Trigger Type", &currentTrigger, Ability::Strings::Trigger::ALL.data(), static_cast<int>(Ability::Strings::Trigger::ALL.size()))) {
      cfg.trigger.type = Ability::Strings::Trigger::ALL[currentTrigger];
      writeCfgToRaw();
      changed = true;
    }
    changed |= std::visit([](auto& t) { return edit(t); }, raw.trigger);

    int currentCooldown = findIndex(cfg.cooldown.type, Ability::Strings::Cooldown::ALL);
    if(ImGui::Combo("Cooldown Type", &currentCooldown, Ability::Strings::Cooldown::ALL.data(), static_cast<int>(Ability::Strings::Cooldown::ALL.size()))) {
      cfg.cooldown.type = Ability::Strings::Cooldown::ALL[currentCooldown];
      writeCfgToRaw();
      changed = true;
    }
    changed |= std::visit([](auto& c) { return edit(c); }, raw.cooldown);

    ImGui::TreePop();
    return changed;
  }

  void update(GameDB db) {
    Config::GameConfig& config = *TableAdapters::getConfig(db).game;
    bool changed = false;

    ImGui::Begin("Abilities");
    changed |= editAbility(config.ability.pushAbility, "Push");
    ImGui::End();

    if(changed) {
      Player::initAbility(db);
    }
  }
}

void GameModule::update(GameDB db) {
  CameraModule::update(db);
  AbilityModule::update(db);

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

  ImGui::SliderFloat("Camera Zoom Speed", &config.camera.cameraZoomSpeed, 0.0f, 1.0f);
  ImGui::SliderFloat("Boundary Spring Force", &config.world.boundarySpringConstant, 0.0f, 1.0f);
  ImGui::SliderFloat("Fragment Goal Distance", &config.fragment.fragmentGoalDistance, 0.0f, 5.0f);
  ImguiExt::inputSizeT("Fragment Rows", &config.fragment.fragmentRows);
  ImguiExt::inputSizeT("Fragment Columns", &config.fragment.fragmentColumns);
  ImGui::End();
}
