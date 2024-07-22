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

  void processStat(IAppBuilder& builder);

  class Builder : public StatEffect::BuilderBase {
  public:
    Builder(AppTaskArgs& args);

    Builder& setMode(FollowMode mode);
    Builder& setTarget(const ElementRef& ref);

  private:
    CommandRow* command{};
    StatEffect::Target* target{};
  };
}

struct FollowTargetByVelocityStatEffectTable : StatEffectBase<
  StatEffect::Target,
  FollowTargetByVelocityStatEffect::CommandRow
> {};
