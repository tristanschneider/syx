#pragma once

#include "stat/StatEffectBase.h"
#include "glm/vec2.hpp"

namespace VelocityStatEffect {
  struct VelocityCommand {
    glm::vec2 linearImpulse{};
    float angularImpulse{};
  };

  struct CommandRow : Row<VelocityCommand> {};

  void processState(IAppBuilder& builder);
};

struct VelocityStatEffectTable : StatEffectBase<
  VelocityStatEffect::CommandRow
> {};
