#pragma once

#include "stat/StatEffectBase.h"

struct GameDB;

namespace DamageStatEffect {
  struct Command {
    float damage{};
  };

  struct CommandRow : Row<Command> {};

  void processStat(IAppBuilder& builder);

  class Builder : public StatEffect::BuilderBase {
  public:
    Builder(AppTaskArgs& args);

    Builder& setDamage(float damage);

  private:
    CommandRow* command{};
  };
};

struct DamageStatEffectTable : StatEffectBase<
  DamageStatEffect::CommandRow
> {};
