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
    if(info.abilityActive) {
      cooldown.currentTime = 0.0f;
    }
    else {
      cooldown.currentTime = std::min(cooldown.currentTime + info.t, cooldown.maxTime);
    }
  }

  void updateCooldown(CooldownType& cooldown, const UpdateInfo& info) {
    return std::visit([&](auto& c) { return updateCooldown(c, info); }, cooldown);
  }

  TriggerResult tryTrigger(InstantTrigger&, const TriggerInfo& info) {
    if(info.isInputDown) {
      return TriggerWithPower{ 1.0f, 1.0f, true };
    }
    return DontTrigger{};
  }

  TriggerResult tryTrigger(ChargeTrigger& trigger, const TriggerInfo& info) {
    //Input is down, charge it up
    if(info.isInputDown) {
      trigger.currentCharge = CurveSolver::advanceTime(trigger.chargeCurve, trigger.currentCharge, info.t);
      trigger.currentDamageCharge = CurveSolver::advanceTime(trigger.damageChargeCurve, trigger.currentDamageCharge, info.t);
      return DontTrigger{ false };
    }
    //Input is up, reset and potentially trigger ability
    const float storedCharge = trigger.currentCharge;
    const float storedDamageCharge = trigger.currentDamageCharge;
    trigger.currentCharge = trigger.currentDamageCharge = 0.0f;

    //Input has been released and was held long enough to trigger ability, trigger it
    if(storedCharge >= trigger.minimumCharge) {
      const float power = CurveSolver::solve(storedCharge, trigger.chargeCurve);
      const float damage = CurveSolver::solve(storedDamageCharge, trigger.damageChargeCurve);
      return TriggerWithPower{ power, damage, false };
    }

    //Input is up and not enough time was banked to trigger an ability, do nothing
    return DontTrigger{ true };
  }

  //TODO: delete this
  TriggerResult tryTrigger(TriggerType& trigger, const TriggerInfo& info) {
    return std::visit([&](auto& t) { return tryTrigger(t, info); }, trigger);
  }

  struct TriggerVisitor {
    TriggerResult operator()(const InstantTrigger&) {
      return TriggerWithPower{ 1.0f, 1.0f, true };
    }

    TriggerResult operator()(const ChargeTrigger& trigger) {
      //Input is up, reset and potentially trigger ability
      const float storedCharge = CurveSolver::advanceTime(trigger.chargeCurve, trigger.currentCharge, chargeSeconds);
      const float storedDamageCharge = CurveSolver::advanceTime(trigger.damageChargeCurve, trigger.currentDamageCharge, chargeSeconds);

      //Input has been released and was held long enough to trigger ability, trigger it
      if(storedCharge >= trigger.minimumCharge) {
        const float power = CurveSolver::solve(storedCharge, trigger.chargeCurve);
        const float damage = CurveSolver::solve(storedDamageCharge, trigger.damageChargeCurve);
        return TriggerWithPower{ power, damage, false };
      }

      //Input is up and not enough time was banked to trigger an ability, do nothing
      return DontTrigger{ true };
    }

    float chargeSeconds{};
  };

  TriggerResult tryTriggerDirectly(const TriggerType& trigger, float chargeSeconds) {
    return std::visit(TriggerVisitor{ chargeSeconds }, trigger);
  }

  float getMinChargeTime(const TriggerType& t) {
    struct Visitor {
      float operator()(const InstantTrigger&) { return 0.0f; }
      float operator()(const ChargeTrigger& t) { return t.minimumCharge; }
    };
    return std::visit(Visitor{}, t);
  }
}