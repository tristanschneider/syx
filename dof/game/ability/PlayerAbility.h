#pragma once

#include "curve/CurveDefinition.h"
#include <variant>

struct CurveDefinition;

namespace Ability {
  namespace Strings {
    namespace Trigger {
      constexpr const char* INSTANT = "instant";
      constexpr const char* CHARGE = "charge";
      constexpr std::array<const char*, 2> ALL = {
        INSTANT,
        CHARGE
      };
    }
    namespace Cooldown {
      constexpr const char* DISABLED = "disabled";
      constexpr std::array<const char*, 1> ALL = {
        DISABLED
      };
    }
  };

  struct InstantTrigger {};
  struct ChargeTrigger {
    float currentCharge{};
    float currentDamageCharge{};
    float minimumCharge{};
    CurveDefinition chargeCurve{};
    CurveDefinition damageChargeCurve{};
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
    bool abilityActive{};
  };
  void updateCooldown(DisabledCooldown& cooldown, const UpdateInfo& info);
  void updateCooldown(CooldownType& cooldown, const UpdateInfo& info);

  struct TriggerInfo {
    float t{};
    bool isInputDown{};
  };
  struct TriggerWithPower {
    float power{};
    float damage{};
    bool resetInput{};
  };
  struct DontTrigger {
    bool resetInput{};
  };
  using TriggerResult = std::variant<DontTrigger, TriggerWithPower>;
  TriggerResult tryTrigger(InstantTrigger& trigger, const TriggerInfo& info);
  TriggerResult tryTrigger(ChargeTrigger& trigger, const TriggerInfo& info);
  TriggerResult tryTrigger(TriggerType& trigger, const TriggerInfo& info);
}