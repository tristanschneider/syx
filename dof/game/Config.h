#pragma once

#include "curve/CurveDefinition.h"
#include "PhysicsConfig.h"
#include "Table.h"

//Plain data config is in config library, any types that contain dependencies on other projects are converted from plain data here

struct PlayerConfig {
  bool drawMove{};
  CurveDefinition linearSpeedCurve;
  CurveDefinition linearForceCurve;
  CurveDefinition angularSpeedCurve;
  CurveDefinition angularForceCurve;

  CurveDefinition linearStoppingSpeedCurve;
  CurveDefinition linearStoppingForceCurve;
  CurveDefinition angularStoppingSpeedCurve;
  CurveDefinition angularStoppingForceCurve;
};

struct GameConfig {
  PlayerConfig player;
  Config::PlayerAbilityConfig ability;
  Config::CameraConfig camera;
  Config::FragmentConfig fragment;
  Config::WorldConfig world;
  PhysicsConfig physics;
};

namespace ConfigConvert {
  inline CurveDefinition toGame(const Config::CurveConfig& cfg) {
    return {
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

  inline Config::CurveConfig toConfig(const CurveDefinition& def) {
    return {
      def.params.scale,
      def.params.offset,
      def.params.duration,
      def.params.flipInput,
      def.params.flipOutput,
      std::string(def.function.name ? def.function.name : "")
    };
  }

  inline PlayerConfig toGame(const Config::PlayerConfig& cfg) {
    return {
      cfg.drawMove,
      toGame(cfg.linearSpeedCurve),
      toGame(cfg.linearForceCurve),
      toGame(cfg.angularSpeedCurve),
      toGame(cfg.angularForceCurve),
      toGame(cfg.linearStoppingSpeedCurve),
      toGame(cfg.linearStoppingForceCurve),
      toGame(cfg.angularStoppingSpeedCurve),
      toGame(cfg.angularStoppingForceCurve)
    };
  }

  inline Config::PlayerConfig toConfig(const PlayerConfig& cfg) {
    return {
      cfg.drawMove,
      toConfig(cfg.linearSpeedCurve),
      toConfig(cfg.linearForceCurve),
      toConfig(cfg.angularSpeedCurve),
      toConfig(cfg.angularForceCurve),
      toConfig(cfg.linearStoppingSpeedCurve),
      toConfig(cfg.linearStoppingForceCurve),
      toConfig(cfg.angularStoppingSpeedCurve),
      toConfig(cfg.angularStoppingForceCurve)
    };
  }

  inline GameConfig toGame(const Config::RawGameConfig& cfg) {
    return {
      toGame(cfg.player),
      cfg.ability,
      cfg.camera,
      cfg.fragment,
      cfg.world,
      cfg.physics
    };
  }

  inline Config::RawGameConfig toConfig(const GameConfig& cfg) {
    return {
      toConfig(cfg.player),
      cfg.ability,
      cfg.camera,
      cfg.fragment,
      cfg.world,
      cfg.physics
    };
  }
}
