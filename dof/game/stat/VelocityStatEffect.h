#pragma once

#include "stat/StatEffectBase.h"
#include "glm/vec2.hpp"

struct GameDB;

namespace VelocityStatEffect {
  struct VelocityCommand {
    glm::vec2 linearImpulse{};
    float angularImpulse{};
  };

  struct CommandRow : Row<VelocityCommand> {};
};

struct VelocityStatEffectTable : StatEffectBase<
  VelocityStatEffect::CommandRow
> {};

namespace StatEffect {
  TaskRange processStat(VelocityStatEffectTable& table, GameDB db);
};