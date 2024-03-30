#include "Precompile.h"
#include "GameModule.h"
#include "Simulation.h"

#include "AppBuilder.h"
#include "curve/CurveSolver.h"
#include "imgui.h"
#include "ImguiExt.h"
#include "TableAdapters.h"
#include "File.h"
#include "config/ConfigIO.h"
#include "ability/PlayerAbility.h"
#include "Player.h"
#include "ImguiModule.h"
#include "scenes/SceneList.h"
#include "SceneNavigator.h"

namespace Toolbox {
  void update(Config::GameConfig& config, FileSystem& fs) {
    if(ImGui::Button("Save Configuration")) {
      std::string buffer = ConfigIO::serializeJSON(config);
      if(File::writeEntireFile(fs, Simulation::getConfigName(), buffer)) {
        printf("Configuration saved\n");
      }
      else {
        printf("Failed to save configuration\n");
      }
    }
  }
}

namespace CameraModule {
  void update(Config::GameConfig& config) {
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
    {
      static ImguiExt::CurveSliders sliders;
      sliders.label = "Charge";
      sliders.durationRange = { 0.0f, 5.0f };
      sliders.offsetRange = { 0.0f, 5.0f };
      sliders.scaleRange = { 0.0f, 5.0f };
      changed |= ImguiExt::curve(t.chargeCurve, sliders);
    }
    {
      static ImguiExt::CurveSliders sliders;
      sliders.label = "Damage";
      sliders.durationRange = { 0.0f, 5.0f };
      sliders.offsetRange = { 0.0f, 50.0f };
      sliders.scaleRange = { 0.0f, 100.0f };
      changed |= ImguiExt::curve(t.damageChargeCurve, sliders);
    }

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

  void update(Config::GameConfig& config, QueryResultRow<GameInput::PlayerInputRow>& input) {
    bool changed = false;

    ImGui::Begin("Abilities");
    changed |= editAbility(config.ability.pushAbility.ability, "Push");
    ImGui::End();

    if(changed) {
      Player::initAbility(config, input);
    }
  }
}

namespace Scenes {
  void update(SceneList::ListNavigator nav) {
    ImGui::Begin("Scenes");
    if(ImGui::Button("Empty")) {
      nav.navigator->navigateTo(nav.scenes->empty);
    }
    if(ImGui::Button("Fragment")) {
      nav.navigator->navigateTo(nav.scenes->fragment);
    }
    if(ImGui::Button("Single Stack")) {
      nav.navigator->navigateTo(nav.scenes->singleStack);
    }
    ImGui::End();
  }
}

void GameModule::update(IAppBuilder& builder) {
  auto task = builder.createTask();
  task.setName("Imgui Game Module").setPinning(AppTaskPinning::MainThread{});
  Config::GameConfig* config = task.query<SharedRow<Config::GameConfig>>().tryGetSingletonElement();
  FileSystem* fs = task.query<SharedRow<FileSystem>>().tryGetSingletonElement();
  auto input = task.query<GameInput::PlayerInputRow>();
  const bool* enabled = ImguiModule::queryIsEnabled(task);
  auto nav = SceneList::createNavigator(task);
  assert(config && fs);

  task.setCallback([config, fs, input, enabled, nav](AppTaskArgs&) mutable {
    if(!*enabled) {
      return;
    }
    Scenes::update(nav);
    CameraModule::update(*config);
    AbilityModule::update(*config, std::get<0>(input.rows));

    ImGui::Begin("Game");

    Toolbox::update(*config, *fs);
    {
      static ImguiExt::CurveSliders sliders;
      sliders.label = "Linear Speed";
      sliders.durationRange = { 0.0f, 1.0f };
      sliders.offsetRange = { 0.0f, 1.24f };
      sliders.scaleRange = { 0.0f, 5.0f };
      ImguiExt::curve(config->player.linearSpeedCurve, sliders);
    }
    {
      static ImguiExt::CurveSliders sliders;
      sliders.label = "Linear Force";
      sliders.durationRange = { 0.0f, 1.0f };
      sliders.offsetRange = { 0.0f, 0.5f };
      sliders.scaleRange = { 0.0f, 1.0f };
      ImguiExt::curve(config->player.linearForceCurve, sliders);
    }
    {
      static ImguiExt::CurveSliders sliders;
      sliders.label = "Linear Stopping Speed";
      sliders.durationRange = { 0.0f, 1.0f };
      sliders.offsetRange = { 0.0f, 1.24f };
      sliders.scaleRange = { 0.0f, 5.0f };
      ImguiExt::curve(config->player.linearStoppingSpeedCurve, sliders);
    }
    {
      static ImguiExt::CurveSliders sliders;
      sliders.label = "Linear Stopping Force";
      sliders.durationRange = { 0.0f, 1.0f };
      sliders.offsetRange = { 0.0f, 0.0f };
      sliders.scaleRange = { 0.0f, 0.25f };
      ImguiExt::curve(config->player.linearStoppingForceCurve, sliders);
    }

    {
      static ImguiExt::CurveSliders sliders;
      sliders.label = "Angular Speed";
      sliders.durationRange = { 0.0f, 1.0f };
      sliders.offsetRange = { 0.0f, 1.24f };
      sliders.scaleRange = { 0.0f, 5.0f };
      ImguiExt::curve(config->player.angularSpeedCurve, sliders);
    }
    {
      static ImguiExt::CurveSliders sliders;
      sliders.label = "Angular Force";
      sliders.durationRange = { 0.0f, 1.0f };
      sliders.offsetRange = { 0.0f, 0.5f };
      sliders.scaleRange = { 0.0f, 1.0f };
      ImguiExt::curve(config->player.angularForceCurve, sliders);
    }
    {
      static ImguiExt::CurveSliders sliders;
      sliders.label = "Angular Stopping Speed";
      sliders.durationRange = { 0.0f, 1.0f };
      sliders.offsetRange = { 0.0f, 0.5f };
      sliders.scaleRange = { 0.0f, 1.0f };
      ImguiExt::curve(config->player.angularStoppingSpeedCurve, sliders);
    }
    {
      static ImguiExt::CurveSliders sliders;
      sliders.label = "Angular Stopping Force";
      sliders.durationRange = { 0.0f, 1.0f };
      sliders.offsetRange = { 0.0f, 0.5f };
      sliders.scaleRange = { 0.0f, 1.0f };
      ImguiExt::curve(config->player.angularStoppingForceCurve, sliders);
    }
    ImGui::Checkbox("Draw Move", &config->player.drawMove);

    ImGui::SliderFloat("Camera Zoom Speed", &config->camera.cameraZoomSpeed, 0.0f, 1.0f);
    ImGui::SliderFloat("Boundary Spring Force", &config->world.boundarySpringConstant, 0.0f, 1.0f);
    ImGui::SliderFloat("Fragment Goal Distance", &config->fragment.fragmentGoalDistance, 0.0f, 5.0f);
    ImguiExt::inputSizeT("Fragment Rows", &config->fragment.fragmentRows);
    ImguiExt::inputSizeT("Fragment Columns", &config->fragment.fragmentColumns);
    ImGui::End();
  });

  builder.submitTask(std::move(task));
}
