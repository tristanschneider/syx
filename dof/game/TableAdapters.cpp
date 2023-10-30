#include "Precompile.h"
#include "TableAdapters.h"

#include "stat/AllStatEffects.h"
#include "Simulation.h"
#include "ThreadLocals.h"
#include "AppBuilder.h"

DebugLineAdapter::~DebugLineAdapter() = default;

const Config::PhysicsConfig* TableAdapters::getPhysicsConfig(RuntimeDatabaseTaskBuilder& task) {
  return &getGameConfig(task)->physics;
}

Config::PhysicsConfig* TableAdapters::getPhysicsConfigMutable(RuntimeDatabaseTaskBuilder& task) {
  return &getGameConfigMutable(task)->physics;
}

const Config::GameConfig* TableAdapters::getGameConfig(RuntimeDatabaseTaskBuilder& task) {
  return task.query<const SharedRow<Config::GameConfig>>().tryGetSingletonElement();
}

Config::GameConfig* TableAdapters::getGameConfigMutable(RuntimeDatabaseTaskBuilder& task) {
  return task.query<SharedRow<Config::GameConfig>>().tryGetSingletonElement();
}

ThreadLocals& TableAdapters::getThreadLocals(RuntimeDatabaseTaskBuilder& task) {
  ThreadLocals& tls = *task.query<ThreadLocalsRow>().tryGetSingletonElement()->instance;
  task.setPinning(AppTaskPinning::Synchronous{});
  return tls;
}

namespace {
  struct StatArgs {
    StatArgs(AppTaskArgs& task)
      : db{ *ThreadLocalData::get(task).statEffects }
      , mappings{ *ThreadLocalData::get(task).mappings } {
    }

    StatEffectDatabase& db;
    StableElementMappings& mappings;
  };

  template<class TableT>
  StatEffectBaseAdapter getStatBase(TableT& table, StatArgs& args) {
    return {
      &std::get<StatEffect::Owner>(table.mRows),
      &std::get<StatEffect::Lifetime>(table.mRows),
      &std::get<StatEffect::Global>(table.mRows),
      &std::get<StatEffect::Continuations>(table.mRows),
      StableTableModifierInstance::get<StatEffectDatabase>(table, args.mappings),
      TableOperations::tryGetRow<StatEffect::Target>(table),
      TableOperations::tryGetRow<StatEffect::CurveInput<>>(table),
      TableOperations::tryGetRow<StatEffect::CurveOutput<>>(table),
      TableOperations::tryGetRow<StatEffect::CurveDef<>>(table)
    };
  }

  template<class TableT>
  TransformAdapter getTransform(TableT& table) {
    return {
      TableOperations::tryGetRow<FloatRow<Tags::Pos, Tags::X>>(table),
      TableOperations::tryGetRow<FloatRow<Tags::Pos, Tags::Y>>(table),
      TableOperations::tryGetRow<FloatRow<Tags::Rot, Tags::CosAngle>>(table),
      TableOperations::tryGetRow<FloatRow<Tags::Rot, Tags::SinAngle>>(table)
    };
  }

  template<class TableT>
  TransformAdapter getGameplayTransform(TableT& table) {
    return {
      TableOperations::tryGetRow<FloatRow<Tags::GPos, Tags::X>>(table),
      TableOperations::tryGetRow<FloatRow<Tags::GPos, Tags::Y>>(table),
      TableOperations::tryGetRow<FloatRow<Tags::GRot, Tags::CosAngle>>(table),
      TableOperations::tryGetRow<FloatRow<Tags::GRot, Tags::SinAngle>>(table)
    };
  }

  template<class TableT>
  PhysicsObjectAdapter getPhysics(TableT& table) {
    return {
      TableOperations::tryGetRow<FloatRow<Tags::LinVel, Tags::X>>(table),
      TableOperations::tryGetRow<FloatRow<Tags::LinVel, Tags::Y>>(table),
      TableOperations::tryGetRow<FloatRow<Tags::AngVel, Tags::Angle>>(table),
      TableOperations::tryGetRow<FloatRow<Tags::GLinImpulse, Tags::X>>(table),
      TableOperations::tryGetRow<FloatRow<Tags::GLinImpulse, Tags::Y>>(table),
      TableOperations::tryGetRow<FloatRow<Tags::GAngImpulse, Tags::Angle>>(table),
      TableOperations::tryGetRow<CollisionMaskRow>(table)
    };
  }

  template<class TableT>
  PhysicsObjectAdapter getGameplayPhysics(TableT& table) {
    return {
      TableOperations::tryGetRow<FloatRow<Tags::GLinVel, Tags::X>>(table),
      TableOperations::tryGetRow<FloatRow<Tags::GLinVel, Tags::Y>>(table),
      TableOperations::tryGetRow<FloatRow<Tags::GAngVel, Tags::Angle>>(table),
      TableOperations::tryGetRow<FloatRow<Tags::GLinImpulse, Tags::X>>(table),
      TableOperations::tryGetRow<FloatRow<Tags::GLinImpulse, Tags::Y>>(table),
      TableOperations::tryGetRow<FloatRow<Tags::GAngImpulse, Tags::Angle>>(table),
      TableOperations::tryGetRow<CollisionMaskRow>(table)
    };
  }

  template<class TableT>
  FragmentAdapter getFragment(TableT& table) {
    return {
      TableOperations::tryGetRow<FloatRow<Tags::FragmentGoal, Tags::X>>(table),
      TableOperations::tryGetRow<FloatRow<Tags::FragmentGoal, Tags::Y>>(table),
      TableOperations::tryGetRow<DamageTaken>(table),
      TableOperations::tryGetRow<Tint>(table),
      TableOperations::tryGetRow<FragmentStateMachine::StateRow>(table)
    };
  }

  template<class TableT>
  StableIDRow* getStableRow(TableT& table) {
    return TableOperations::tryGetRow<StableIDRow>(table);
  }

  PositionStatEffectAdapter getPositionEffects(StatArgs args) {
    auto& table = std::get<PositionStatEffectTable>(args.db.mTables);
    return {
      getStatBase(table, args),
      &std::get<PositionStatEffect::CommandRow>(table.mRows),
    };
  }

  VelocityStatEffectAdapter getVelocityEffects(StatArgs args) {
    auto& table = std::get<VelocityStatEffectTable>(args.db.mTables);
    return {
      getStatBase(table, args),
      &std::get<VelocityStatEffect::CommandRow>(table.mRows),
    };
  }

  LambdaStatEffectAdapter getLambdaEffects(StatArgs args) {
    auto& table = std::get<LambdaStatEffectTable>(args.db.mTables);
    return {
      getStatBase(table, args),
      &std::get<LambdaStatEffect::LambdaRow>(table.mRows),
    };
  }

  AreaForceStatEffectAdapter getAreaForceEffects(StatArgs args) {
    auto& table = std::get<AreaForceStatEffectTable>(args.db.mTables);
    return {
      getStatBase(table, args),
      &std::get<AreaForceStatEffect::CommandRow>(table.mRows)
    };
  }

  FollowTargetByPositionStatEffectAdapter getFollowTargetByPositionEffects(StatArgs args) {
    auto& table = std::get<FollowTargetByPositionStatEffectTable>(args.db.mTables);
    return {
      getStatBase(table, args),
      &std::get<FollowTargetByPositionStatEffect::CommandRow>(table.mRows)
    };
  }

  FollowTargetByVelocityStatEffectAdapter getFollowTargetByVelocityEffects(StatArgs args) {
    auto& table = std::get<FollowTargetByVelocityStatEffectTable>(args.db.mTables);
    return {
      getStatBase(table, args),
      &std::get<FollowTargetByVelocityStatEffect::CommandRow>(table.mRows)
    };
  }

  DamageStatEffectAdapter getDamageEffects(StatArgs args) {
    auto& table = std::get<DamageStatEffectTable>(args.db.mTables);
    return {
      getStatBase(table, args),
      &std::get<DamageStatEffect::CommandRow>(table.mRows)
    };
  }
}

PositionStatEffectAdapter TableAdapters::getPositionEffects(AppTaskArgs& args) {
  return ::getPositionEffects(args);
}

VelocityStatEffectAdapter TableAdapters::getVelocityEffects(AppTaskArgs& args) {
  return ::getVelocityEffects(args);
}

LambdaStatEffectAdapter TableAdapters::getLambdaEffects(AppTaskArgs& args) {
  return ::getLambdaEffects(args);
}

AreaForceStatEffectAdapter TableAdapters::getAreaForceEffects(AppTaskArgs& args) {
  return ::getAreaForceEffects(args);
}

FollowTargetByPositionStatEffectAdapter TableAdapters::getFollowTargetByPositionEffects(AppTaskArgs& args) {
  return ::getFollowTargetByPositionEffects(args);
}

FollowTargetByVelocityStatEffectAdapter TableAdapters::getFollowTargetByVelocityEffects(AppTaskArgs& args) {
  return ::getFollowTargetByVelocityEffects(args);
}

DamageStatEffectAdapter TableAdapters::getDamageEffects(AppTaskArgs& args) {
  return ::getDamageEffects(args);
}

TransformAdapter TableAdapters::getTransform(RuntimeDatabaseTaskBuilder& task, const UnpackedDatabaseElementID& table) {
  TransformAdapter result;
  std::tie(result.posX, result.posY, result.rotX, result.rotY) = task.query<
    FloatRow<Tags::Pos, Tags::X>, FloatRow<Tags::Pos, Tags::Y>,
    FloatRow<Tags::Rot, Tags::CosAngle>, FloatRow<Tags::Rot, Tags::SinAngle>
  >(table).get(0);
  return result;
}

TransformAdapter TableAdapters::getGameplayTransform(RuntimeDatabaseTaskBuilder& task, const UnpackedDatabaseElementID& table) {
  TransformAdapter result;
  std::tie(result.posX, result.posY, result.rotX, result.rotY) = task.query<
    FloatRow<Tags::GPos, Tags::X>, FloatRow<Tags::Pos, Tags::Y>,
    FloatRow<Tags::GRot, Tags::CosAngle>, FloatRow<Tags::Rot, Tags::SinAngle>
  >(table).get(0);
  return result;
}

size_t TableAdapters::addStatEffectsSharedLifetime(StatEffectBaseAdapter& base, size_t lifetime, const size_t* stableIds, size_t count) {
  const size_t firstIndex = base.modifier.addElements(count, nullptr);
  for(size_t i = 0; i < count; ++i) {
    base.lifetime->at(i + firstIndex) = lifetime;
    base.owner->at(i + firstIndex) = stableIds ? StableElementID::fromStableID(stableIds[i]) : StableElementID::invalid();
  }
  return firstIndex;
}

DebugLineAdapter TableAdapters::getDebugLines(RuntimeDatabaseTaskBuilder& task) {
  auto query = task.query<Row<DebugPoint>>();
  DebugLineAdapter result;
  result.points = &query.get<0>(0);
  result.modifier = task.getModifierForTable(query.matchingTableIDs[0]);
  return result;
}

const float* TableAdapters::getDeltaTime(RuntimeDatabaseTaskBuilder& task) {
  return &getGameConfig(task)->world.deltaTime;
}
