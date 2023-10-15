#pragma once

#include "stat/StatEffectBase.h"
#include "glm/vec2.hpp"

struct GameDB;

//This effect is unique in that its target has no meaning because it affects an area
//It still uses the same deferred processing and lifetime as other effects
namespace AreaForceStatEffect {
  struct Command {
    struct Cone {
      float halfAngle{};
      float length{};
    };
    using Variant = std::variant<Cone>;

    struct FlatImpulse {
      float multiplier{};
    };
    using ImpulseType = std::variant<FlatImpulse>;

    glm::vec2 origin{};
    glm::vec2 direction{};
    //The shape describes the area that will be subdivided into many rays where the base of
    //the shape is at the given position and oriented in the direction, then several rays
    //are tested along the shape in the direction
    Variant shape;
    ImpulseType impulseType;
    //The amount of distance that a ray can go through before objects behind it cannot be affected
    //Split between moving objects an terrain
    float dynamicPiercing{};
    float terrainPiercing{};
    float damage{};
    //Number of rays that the shape is subdivided into
    size_t rayCount{};
  };

  struct CommandRow : Row<Command> {};

  void processStat(IAppBuilder& builder);
};

struct AreaForceStatEffectTable : StatEffectBase<
  AreaForceStatEffect::CommandRow
> {};
