#pragma once

#include "stat/StatEffectBase.h"
#include "glm/vec2.hpp"

// When this is on an object, the next time it collides with an object with CanTriggerFragmentBurstRow
// it will move all nearby fragments from the static table back out to the dynamic table
namespace FragmentBurstStatEffect {
  struct Command {
    float radius{};
  };
  struct CommandRow : Row<Command> {};
  // Put this in any table for which collision with an object with the effect should trigger the burst
  struct CanTriggerFragmentBurstRow : TagRow {};

  void processStat(IAppBuilder& builder);

  class Builder : public StatEffect::BuilderBase {
  public:
    Builder(AppTaskArgs& args);

    Builder& setRadius(float radius);

  private:
    CommandRow* command{};
  };
};

struct FragmentBurstStatEffectTable : StatEffectBase<
  FragmentBurstStatEffect::CommandRow
> {};
