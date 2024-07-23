#pragma once

#include "stat/StatEffectBase.h"
#include "glm/vec2.hpp"

struct GameDB;

namespace PositionStatEffect {
  struct PositionCommand {
    std::optional<glm::vec2> pos{};
    std::optional<glm::vec2> rot{};
    std::optional<float> posZ;
  };

  struct CommandRow : Row<PositionCommand> {};

  void processStat(IAppBuilder& builder);

  class Builder : public StatEffect::BuilderBase {
  public:
    Builder(AppTaskArgs& args);

    Builder& setZ(float z);
    Builder& setPos(const glm::vec2& p);
    Builder& setRot(const glm::vec2& r);

  private:
    CommandRow* command{};
  };
};

struct PositionStatEffectTable : StatEffectBase<
  PositionStatEffect::CommandRow
> {};
