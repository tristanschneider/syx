#pragma once

#include "stat/StatEffectBase.h"
#include "glm/vec2.hpp"

struct GameDB;

namespace FollowTargetByPositionStatEffect {
  enum class FollowMode {
    //Advance a percentage towards the goal every frame
    Interpolation,
    //Advance a distance twoards the goal every frame
    Movement
  };
  struct Command {
    FollowMode mode{};
  };

  struct CommandRow : Row<Command>{};

  void processStat(IAppBuilder& builder);

  class Builder : public StatEffect::BuilderBase {
  public:
    Builder(AppTaskArgs& args);

    Builder& setMode(FollowMode mode);
    Builder& setTarget(const ElementRef& ref);
    Builder& setCurve(CurveDefinition& c);

  private:
    CommandRow* command{};
    StatEffect::Target* target{};
    StatEffect::CurveDef<>* curve{};
  };
}

struct FollowTargetByPositionStatEffectTable : StatEffectBase<
  StatEffect::Target,
  StatEffect::CurveInput<>,
  StatEffect::CurveOutput<>,
  StatEffect::CurveDef<>,
  FollowTargetByPositionStatEffect::CommandRow
> {};
