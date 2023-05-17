#pragma once

#include "stat/StatEffectBase.h"

struct GameDB;

//This effect is unique in that its target has no meaning because it affects an area
//It still uses the same deferred processing and lifetime as other effects
namespace AreaForceStatEffect {
  struct PointX : Row<float> {};
  struct PointY : Row<float> {};
  struct Strength : Row<float> {};
};

struct AreaForceStatEffectTable : StatEffectBase<
  AreaForceStatEffect::PointX,
  AreaForceStatEffect::PointY,
  AreaForceStatEffect::Strength
> {};

namespace StatEffect {
  TaskRange processStat(AreaForceStatEffectTable& table, GameDB db);
}