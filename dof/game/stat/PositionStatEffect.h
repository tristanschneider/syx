#pragma once

#include "stat/StatEffectBase.h"
#include "glm/vec2.hpp"

struct GameDB;

namespace PositionStatEffect {
  struct PositionCommand {
    std::optional<glm::vec2> pos{};
    std::optional<glm::vec2> rot{};
  };

  struct CommandRow : Row<PositionCommand> {};

  void processStat(IAppBuilder& builder);
};

struct PositionStatEffectTable : StatEffectBase<
  PositionStatEffect::CommandRow
> {};
