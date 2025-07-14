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

  FloatAlias posX;
  FloatAlias posY;
  FloatAlias rotX;
  FloatAlias rotY;
  FloatAlias scaleX;
  FloatAlias scaleY;
  FloatAlias linVelX;
  FloatAlias linVelY;
  FloatAlias angVel;

  //Optional, without this bodies are considered to be at z=0 and immobile along z
  FloatAlias posZ;
  FloatAlias linVelZ;

  FloatAlias broadphaseMinX;
  FloatAlias broadphaseMinY;
  FloatAlias broadphaseMaxX;
  FloatAlias broadphaseMaxY;
};

namespace Physics {
  constexpr float DEFAULT_Z = 0;

  //TODO: expose createDatabase here rather than having gameplay create the physics tables

  std::unique_ptr<IAppModule> createModule(const PhysicsAliases& aliases);

  void integrateVelocity(IAppBuilder& builder, const PhysicsAliases& aliases);
  void integratePosition(IAppBuilder& builder, const PhysicsAliases& aliases);
  void integrateRotation(IAppBuilder& builder, const PhysicsAliases& aliases);
  void applyDampingMultiplier(IAppBuilder& builder, const PhysicsAliases& aliases, const float& linearMultiplier, const float& angularMultiplier);
};