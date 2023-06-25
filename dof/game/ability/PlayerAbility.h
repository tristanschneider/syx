#pragma once

#include <variant>

struct CurveDefinition;

namespace Ability {
  struct InstantTrigger {};
  struct ChargeTrigger {
    float currentCharge{};
    float minimumCharge{};
    const CurveDefinition* chargeCurve{};
  };
  using TriggerType = std::variant<InstantTrigger, ChargeTrigger>;

  struct DisabledCooldown {
    float currentTime{};
    float maxTime{};
  };
  using CooldownType = std::variant<DisabledCooldown>;

  struct AbilityInput {
    TriggerType trigger;
    CooldownType cooldown;
  };

  bool isOnCooldown(const DisabledCooldown& cooldown);
  bool isOnCooldown(const CooldownType& cooldown);

  struct UpdateInfo {
    float t{};
  };
  void updateCooldown(DisabledCooldown& cooldown, const UpdateInfo& info);
  void updateCooldown(CooldownType& cooldown, const UpdateInfo& info);

  struct TriggerInfo {
    float t{};
    bool isInputDown{};
  };
  struct TriggerWithPower {
    float power{};
  };
  struct DontTrigger {};
  using TriggerResult = std::variant<DontTrigger, TriggerWithPower>;
  TriggerResult tryTrigger(InstantTrigger& trigger, const TriggerInfo& info);
  TriggerResult tryTrigger(ChargeTrigger& trigger, const TriggerInfo& info);
  TriggerResult tryTrigger(TriggerType& trigger, const TriggerInfo& info);
}