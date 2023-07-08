#pragma once

#include "stat/StatEffectBase.h"

struct GameDB;

namespace DamageStatEffect {
  struct Command {
    float damage{};
  };

  struct CommandRow : Row<Command> {};
};

struct DamageStatEffectTable : StatEffectBase<
  DamageStatEffect::CommandRow
> {};

namespace StatEffect {
  TaskRange processStat(DamageStatEffectTable& table, GameDB db);
}