#pragma once

#include "Profile.h"
#include "glm/vec2.hpp"
#include "config/Config.h"
#include "Scheduler.h"
#include "Table.h"
#include "AppBuilder.h"

class IAppModule;

struct ZeroMassObjectTableTag : SharedRow<char> {};
struct SpatialQueriesTableTag : SharedRow<char> {};

struct AccelerationX : Row<float> {};
struct AccelerationY : Row<float> {};
struct AccelerationZ : Row<float> {};

struct PhysicsAliases {
  using FloatAlias = QueryAlias<Row<float>>;
  using TagAlias = QueryAlias<TagRow>;

  FloatAlias linVelX;
  FloatAlias linVelY;
  FloatAlias angVel;

  //Optional, without this bodies are considered to be at z=0 and immobile along z
  FloatAlias linVelZ;
};

namespace Physics {
  constexpr float DEFAULT_Z = 0;

  //TODO: expose createDatabase here rather than having gameplay create the physics tables

  std::unique_ptr<IAppModule> createModule();

  void integrateVelocity(IAppBuilder& builder, const PhysicsAliases& aliases);
  void integratePositionAndRotation(IAppBuilder& builder, const PhysicsAliases& aliases);
  void applyDampingMultiplier(IAppBuilder& builder, const PhysicsAliases& aliases, const float& linearMultiplier, const float& angularMultiplier);
};