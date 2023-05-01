#pragma once

#include "stat/StatEffectBase.h"

struct GameDB;

namespace LambdaStatEffect {
  struct Args {
    GameDB* db{};
    StableElementID resolvedID;
  };
  using Lambda = std::function<void(Args&)>;
  struct LambdaRow : Row<Lambda> {};
};

struct LambdaStatEffectTable : StatEffectBase<
  LambdaStatEffect::LambdaRow
> {};

namespace StatEffect {
  TaskRange processStat(LambdaStatEffectTable& table, GameDB db);
};
