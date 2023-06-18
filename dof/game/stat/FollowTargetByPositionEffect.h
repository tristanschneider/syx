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
}

struct FollowTargetByPositionStatEffectTable : StatEffectBase<
  StatEffect::Target,
  StatEffect::CurveInput<>,
  StatEffect::CurveOutput<>,
  StatEffect::CurveDef<>,
  FollowTargetByPositionStatEffect::CommandRow
> {};

namespace StatEffect {
  TaskRange processStat(FollowTargetByPositionStatEffectTable& table, GameDB db);
}