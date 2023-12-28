#pragma once

#include "Profile.h"
#include "glm/vec2.hpp"
#include "config/Config.h"
#include "Queries.h"
#include "Scheduler.h"
#include "Table.h"
#include "AppBuilder.h"

struct ZeroMassObjectTableTag : SharedRow<char> {};
struct SpatialQueriesTableTag : SharedRow<char> {};

struct PhysicsAliases {
  using FloatAlias = QueryAlias<Row<float>>;
  using TagAlias = QueryAlias<TagRow>;

  FloatAlias posX;
  FloatAlias posY;
  FloatAlias rotX;
  FloatAlias rotY;
  FloatAlias linVelX;
  FloatAlias linVelY;
  FloatAlias angVel;

  FloatAlias broadphaseMinX;
  FloatAlias broadphaseMinY;
  FloatAlias broadphaseMaxX;
  FloatAlias broadphaseMaxY;

  TagAlias isImmobile;
};

namespace Physics {
  //TODO: expose createDatabase here rather than having gameplay create the physics tables

  void integratePosition(IAppBuilder& builder, const PhysicsAliases& aliases);
  void integrateRotation(IAppBuilder& builder, const PhysicsAliases& aliases);
  void applyDampingMultiplier(IAppBuilder& builder, const PhysicsAliases& aliases, const float& linearMultiplier, const float& angularMultiplier);
};