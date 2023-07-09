#pragma once

#include "stat/StatEffectBase.h"
#include "glm/vec2.hpp"

struct GameDB;

namespace FollowTargetByVelocityStatEffect {
  struct SpringFollow {
    float springConstant{};
  };
  using FollowMode = std::variant<SpringFollow>;
  struct Command {
    FollowMode mode;
  };

  struct CommandRow : Row<Command>{};
}

struct FollowTargetByVelocityStatEffectTable : StatEffectBase<
  StatEffect::Target,
  FollowTargetByVelocityStatEffect::CommandRow
> {};

namespace StatEffect {
  TaskRange processStat(FollowTargetByVelocityStatEffectTable& table, GameDB db);
}