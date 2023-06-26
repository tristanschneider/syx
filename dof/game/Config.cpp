#include "Precompile.h"
#include "Config.h"

#include "ability/PlayerAbility.h"

namespace Config {
  struct GameCurveConfig : Config::ICurveAdapter {
    Config::CurveConfig read() const override {
      return {
        definition.params.scale,
        definition.params.offset,
        definition.params.duration,
        definition.params.flipInput,
        definition.params.flipOutput,
        std::string(definition.function.name ? definition.function.name : "")
      };
    }

    void write(const Config::CurveConfig& cfg) override {
      definition = {
        CurveParameters {
          cfg.scale,
          cfg.offset,
          cfg.duration,
          cfg.flipInput,
          cfg.flipOutput
        },
        CurveMath::tryGetFunction(cfg.curveFunction)
      };
    }

    CurveDefinition& get() { return definition; }
    const CurveDefinition& get() const { return definition; }

    CurveDefinition definition;
  };

  struct GameAbilityConfig : IAdapter<Config::AbilityConfig> {
    static void read(const Ability::DisabledCooldown& c, Config::AbilityCooldownConfig& result) {
      result.type = Ability::Strings::Cooldown::DISABLED;
      result.maxTime = c.maxTime;
    }

    static void write(const Config::AbilityCooldownConfig& c, Ability::CooldownType& result) {
      if(c.type == Ability::Strings::Cooldown::DISABLED) {
        result = Ability::DisabledCooldown { c.maxTime };
      }
    }

    static void read(const Ability::InstantTrigger&, Config::AbilityTriggerConfig& result) {
      result.type = Ability::Strings::Trigger::INSTANT;
    }

    static void read(const Ability::ChargeTrigger& t, Config::AbilityTriggerConfig& result) {
      result.type = Ability::Strings::Trigger::CHARGE;
      result.minCharge = t.minimumCharge;
      GameCurveConfig temp;
      temp.definition = t.chargeCurve;
      result.chargeCurve =  temp.read();
    }

    static void write(const Config::AbilityTriggerConfig& t, Ability::TriggerType& result) {
      if(t.type == Ability::Strings::Trigger::INSTANT) {
        result = Ability::InstantTrigger{};
      }
      else if(t.type == Ability::Strings::Trigger::CHARGE) {
        GameCurveConfig temp;
        temp.write(t.chargeCurve);
        result = Ability::ChargeTrigger {
          0.0f,
          t.minCharge,
          temp.definition
        };
      }
    }

    Config::AbilityConfig read() const override {
      Config::AbilityConfig result;
      std::visit([&](const auto& c) { read(c, result.cooldown); }, value.cooldown);
      std::visit([&](const auto& t) { read(t, result.trigger); }, value.trigger);
      return result;
    }

    void write(const Config::AbilityConfig& cfg) override {
      write(cfg.trigger, value.trigger);
      write(cfg.cooldown, value.cooldown);
    }

    Ability::AbilityInput& get() { return value; }
    const Ability::AbilityInput& get() const { return value; }

    Ability::AbilityInput value;
  };

  struct GameFactory : Config::IFactory {
    void init(Config::CurveConfigExt& curve) const override {
      curve.adapter = std::make_unique<GameCurveConfig>();
    }

    void init(Config::AbilityConfigExt& ability) const override {
      ability.adapter = std::make_unique<GameAbilityConfig>();
    }
  };

  std::unique_ptr<Config::IFactory> createFactory() {
    return std::make_unique<GameFactory>();
  }

  template<class AdapterT, class T>
  auto& getExt(Config::ConfigExt<T>& ext) {
    if(!ext.adapter) {
      GameFactory{}.init(ext);
    }
    return static_cast<AdapterT&>(*ext.adapter).get();
  }

  template<class AdapterT, class T>
  const auto& getExt(const Config::ConfigExt<T>& ext) {
    return static_cast<const AdapterT&>(*ext.adapter).get();
  }


  Ability::AbilityInput& getAbility(AbilityConfigExt& ext) {
    return getExt<GameAbilityConfig>(ext);
  }

  const CurveDefinition& getCurve(const CurveConfigExt& ext) {
    return getExt<GameCurveConfig>(ext);
  }

  CurveDefinition& getCurve(CurveConfigExt& ext) {
    return getExt<GameCurveConfig>(ext);
  }
}