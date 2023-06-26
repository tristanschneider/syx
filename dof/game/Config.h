#pragma once

#include "curve/CurveDefinition.h"
#include "config/Config.h"
#include <memory>
#include "Table.h"

//Plain data config is in config library, any types that contain dependencies on other projects are converted from plain data here
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

  CurveDefinition definition;
};

namespace Config {
  inline CurveDefinition& getCurve(CurveConfigExt& ext) {
    if(!ext.adapter) {
      ext.adapter = std::make_unique<GameCurveConfig>();
    }
    return static_cast<GameCurveConfig&>(*ext.adapter).definition;
  }
  inline const CurveDefinition& getCurve(const CurveConfigExt& ext) {
    return static_cast<const GameCurveConfig&>(*ext.adapter).definition;
  }
}

struct GameConfigFactory : Config::IFactory {
  virtual Config::CurveConfigExt createCurve() const override {
    return { std::make_unique<GameCurveConfig>() };
  }
};