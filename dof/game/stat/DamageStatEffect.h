#pragma once

#include "stat/StatEffectBase.h"

struct GameDB;

namespace DamageStatEffect {
  struct Command {
    float damage{};
  };

  struct CommandRow : Row<Command> {};

  void processStat(IAppBuilder& builder);
};

struct DamageStatEffectTable : StatEffectBase<
  DamageStatEffect::CommandRow
> {};
