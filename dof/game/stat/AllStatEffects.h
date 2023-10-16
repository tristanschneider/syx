#pragma once

#include "stat/AreaForceStatEffect.h"
#include "stat/DamageStatEffect.h"
#include "stat/FollowTargetByPositionEffect.h"
#include "stat/FollowTargetByVelocityEffect.h"
#include "stat/LambdaStatEffect.h"
#include "stat/PositionStatEffect.h"
#include "stat/VelocityStatEffect.h"

namespace AllStatEffects {
  struct Globals {
    StableElementMappings stableMappings;
  };
  struct GlobalRow : SharedRow<Globals> {};
  struct GlobalTable : Table<GlobalRow> {};
}

using StatEffectDatabase = Database<
  AllStatEffects::GlobalTable,
  LambdaStatEffectTable,
  PositionStatEffectTable,
  VelocityStatEffectTable,
  AreaForceStatEffectTable,
  FollowTargetByPositionStatEffectTable,
  FollowTargetByVelocityStatEffectTable,
  DamageStatEffectTable
>;

// To allow forward declarations
struct StatEffectDBOwned {
  StatEffectDatabase db;
};

struct StatEffectDB {
  StatEffectDatabase& db;
};

struct AllStatTasks {
  //Requires mutable access to position and rotation
  TaskRange positionSetters;
  //Requires mutable access to linear and angular velocity
  TaskRange velocitySetters;
  //Read position set velocity
  TaskRange posGetVelSet;
  //Requires exclusive access to the gameplay portion of the database, can add/remove elements
  TaskRange synchronous;
};

namespace StatEffect {
  //Remove all elements of 'from' and put them in 'to'
  //Intended to be used to move newly created thread local effects to the central database
  void moveThreadLocalToCentral(IAppBuilder& builder);
  void createTasks(IAppBuilder& builder);

  AllStatEffects::Globals& getGlobals(StatEffectDatabase& db);
};