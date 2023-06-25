#include "Precompile.h"
#include "ability/PlayerAbility.h"
#include "curve/CurveSolver.h"
#include <cassert>

namespace Ability {
  bool isOnCooldown(const DisabledCooldown& cooldown) {
    return cooldown.currentTime < cooldown.maxTime;
  }

  bool isOnCooldown(const CooldownType& cooldown) {
    return std::visit([](const auto& c) { return isOnCooldown(c); }, cooldown);
  }

  void updateCooldown(DisabledCooldown& cooldown, const UpdateInfo& info) {
    cooldown.currentTime = std::min(cooldown.currentTime + info.t, cooldown.maxTime);
  }

  void updateCooldown(CooldownType& cooldown, const UpdateInfo& info) {
    return std::visit([&](auto& c) { return updateCooldown(c, info); }, cooldown);
  }

  TriggerResult tryTrigger(InstantTrigger&, const TriggerInfo& info) {
    if(info.isInputDown) {
      return TriggerWithPower{ 1.0f };
    }
    return DontTrigger{};
  }

  TriggerResult tryTrigger(ChargeTrigger& trigger, const TriggerInfo& info) {
    assert(trigger.chargeCurve != nullptr && "Charge curve should be set if ability has charge trigger");
    if(!trigger.chargeCurve) {
      return DontTrigger{};
    }
    //Input is down, charge it up
    if(info.isInputDown) {
      trigger.currentCharge = CurveSolver::advanceTime(*trigger.chargeCurve, trigger.currentCharge, info.t);
      return DontTrigger{};
    }
    //Input is up, reset and potentially trigger ability
    const float storedCharge = trigger.currentCharge;
    trigger.currentCharge = 0.0f;

    //Input has been released and was held long enough to trigger ability, trigger it
    if(trigger.currentCharge >= trigger.minimumCharge) {
      const float power = CurveSolver::solve(storedCharge, *trigger.chargeCurve);
      return TriggerWithPower{ power };
    }

    //Input is up and not enough time was banked to trigger an ability, do nothing
    return DontTrigger{};
  }

  TriggerResult tryTrigger(TriggerType& trigger, const TriggerInfo& info) {
    return std::visit([&](auto& t) { return tryTrigger(t, info); }, trigger);
  }
}