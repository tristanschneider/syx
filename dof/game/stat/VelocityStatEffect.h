#pragma once

#include "stat/StatEffectBase.h"
#include "glm/vec2.hpp"

namespace VelocityStatEffect {
  struct ImpulseCommand {
    glm::vec2 linearImpulse{};
    float angularImpulse{};
    float impulseZ{};
  };
  struct SetZCommand {
    float z{};
  };
  struct VelocityCommand {
    using Variant = std::variant<ImpulseCommand, SetZCommand>;
    Variant data;
  };

  struct CommandRow : Row<VelocityCommand> {};

  void processStat(IAppBuilder& builder);

  class Builder : public StatEffect::BuilderBase {
  public:
    Builder(AppTaskArgs& args);

    Builder& addImpulse(const ImpulseCommand& cmd);
    Builder& setZ(const SetZCommand& cmd);

  private:
    CommandRow* command{};
  };
};

struct VelocityStatEffectTable : StatEffectBase<
  VelocityStatEffect::CommandRow
> {};
