#pragma once

#include "stat/AreaForceStatEffect.h"
#include "stat/DamageStatEffect.h"
#include "stat/FollowTargetByPositionEffect.h"
#include "stat/FollowTargetByVelocityEffect.h"
#include "stat/FragmentBurstStatEffect.h"
#include "stat/LambdaStatEffect.h"
#include "stat/PositionStatEffect.h"
#include "stat/VelocityStatEffect.h"

struct StatEffectDatabase : Database<
  LambdaStatEffectTable,
  PositionStatEffectTable,
  VelocityStatEffectTable,
  AreaForceStatEffectTable,
  FollowTargetByPositionStatEffectTable,
  FollowTargetByVelocityStatEffectTable,
  FragmentBurstStatEffectTable,
  DamageStatEffectTable
> {
  StatEffectDatabase();

  static StatEffectDatabase& get(AppTaskArgs& task);
  static StableElementMappings& getMappings(AppTaskArgs& task);

  template<class TableT>
  static StatEffect::BuilderBase::Args<TableT> createBuilderBase(AppTaskArgs& task) {
    return { std::get<TableT>(get(task).mTables), UnpackedDatabaseElementID::fromPacked(getTableIndex<TableT>()), getMappings(task) };
  }
};

namespace StatEffect {
  //Remove all elements of 'from' and put them in 'to'
  //Intended to be used to move newly created thread local effects to the central database
  void moveThreadLocalToCentral(IAppBuilder& builder);
  void createTasks(IAppBuilder& builder);
};