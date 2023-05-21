#include "Precompile.h"
#include "TableAdapters.h"

#include "stat/AllStatEffects.h"
#include "Simulation.h"
#include "ThreadLocals.h"

ConfigAdapter TableAdapters::getConfig(GameDB db) {
  auto& c = std::get<GlobalGameData>(db.db.mTables);
  GameConfig& g = std::get<SharedRow<GameConfig>>(c.mRows).at();
  return {
    &g.physics,
    &g
  };
}

StableElementMappings& TableAdapters::getStableMappings(GameDB db) {
  return std::get<SharedRow<StableElementMappings>>(std::get<GlobalGameData>(db.db.mTables).mRows).at();
}

ThreadLocals& TableAdapters::getThreadLocals(GameDB db) {
  return *std::get<ThreadLocalsRow>(std::get<GlobalGameData>(db.db.mTables).mRows).at().instance;
}

ThreadLocalData TableAdapters::getThreadLocal(GameDB db, size_t thread) {
  return getThreadLocals(db).get(thread);
}

ExternalDatabases& TableAdapters::getExternalDBs(GameDB db) {
  return std::get<ExternalDatabasesRow>(std::get<GlobalGameData>(db.db.mTables).mRows).at();
}

StatEffectDBOwned& TableAdapters::getStatEffects(GameDB db) {
  return *getExternalDBs(db).statEffects;
}

namespace {
  template<class TableT>
  StatEffectBaseAdapter getStatBase(TableT& table, StatEffectDatabase& db) {
    return {
      &std::get<StatEffect::Owner>(table.mRows),
      &std::get<StatEffect::Lifetime>(table.mRows),
      &std::get<StatEffect::Global>(table.mRows),
      StableTableModifierInstance::get<StatEffectDatabase>(table, StatEffect::getGlobals(db).stableMappings)
    };
  }

  template<class TableT>
  TransformAdapter getTransform(TableT& table) {
    return {
      &std::get<FloatRow<Tags::Pos, Tags::X>>(table.mRows),
      &std::get<FloatRow<Tags::Pos, Tags::Y>>(table.mRows),
      &std::get<FloatRow<Tags::Rot, Tags::CosAngle>>(table.mRows),
      &std::get<FloatRow<Tags::Rot, Tags::SinAngle>>(table.mRows)
    };
  }

  template<class TableT>
  TransformAdapter getGameplayTransform(TableT& table) {
    return {
      &std::get<FloatRow<Tags::GPos, Tags::X>>(table.mRows),
      &std::get<FloatRow<Tags::GPos, Tags::Y>>(table.mRows),
      &std::get<FloatRow<Tags::GRot, Tags::CosAngle>>(table.mRows),
      &std::get<FloatRow<Tags::GRot, Tags::SinAngle>>(table.mRows)
    };
  }

  template<class TableT>
  PhysicsObjectAdapter getPhysics(TableT& table) {
    return {
      &std::get<FloatRow<Tags::LinVel, Tags::X>>(table.mRows),
      &std::get<FloatRow<Tags::LinVel, Tags::Y>>(table.mRows),
      &std::get<FloatRow<Tags::AngVel, Tags::Angle>>(table.mRows),
      &std::get<FloatRow<Tags::GLinImpulse, Tags::X>>(table.mRows),
      &std::get<FloatRow<Tags::GLinImpulse, Tags::Y>>(table.mRows),
      &std::get<FloatRow<Tags::GAngImpulse, Tags::Angle>>(table.mRows),
    };
  }

  template<class TableT>
  PhysicsObjectAdapter getGameplayPhysics(TableT& table) {
    return {
      &std::get<FloatRow<Tags::GLinVel, Tags::X>>(table.mRows),
      &std::get<FloatRow<Tags::GLinVel, Tags::Y>>(table.mRows),
      &std::get<FloatRow<Tags::GAngVel, Tags::Angle>>(table.mRows)
    };
  }

  template<class TableT>
  StableIDRow* getStableRow(TableT& table) {
    return &std::get<StableIDRow>(table.mRows);
  }

  PositionStatEffectAdapter getPositionEffects(StatEffectDatabase& db) {
    auto& table = std::get<PositionStatEffectTable>(db.mTables);
    return {
      getStatBase(table, db),
      &std::get<PositionStatEffect::CommandRow>(table.mRows),
    };
  }

  VelocityStatEffectAdapter getVelocityEffects(StatEffectDatabase& db) {
    auto& table = std::get<VelocityStatEffectTable>(db.mTables);
    return {
      getStatBase(table, db),
      &std::get<VelocityStatEffect::CommandRow>(table.mRows),
    };
  }

  LambdaStatEffectAdapter getLambdaEffects(StatEffectDatabase& db) {
    auto& table = std::get<LambdaStatEffectTable>(db.mTables);
    return {
      getStatBase(table, db),
      &std::get<LambdaStatEffect::LambdaRow>(table.mRows),
    };
  }

  AreaForceStatEffectAdapter getAreaForceEffects(StatEffectDatabase& db) {
    auto& table = std::get<AreaForceStatEffectTable>(db.mTables);
    return {
      getStatBase(table, db),
      &std::get<AreaForceStatEffect::PointX>(table.mRows),
      &std::get<AreaForceStatEffect::PointY>(table.mRows),
      &std::get<AreaForceStatEffect::Strength>(table.mRows),
    };
  }
}

PositionStatEffectAdapter TableAdapters::getPositionEffects(GameDB db, size_t thread) {
  return ::getPositionEffects(getThreadLocal(db, thread).statEffects->db);
}

VelocityStatEffectAdapter TableAdapters::getVelocityEffects(GameDB db, size_t thread) {
  return ::getVelocityEffects(getThreadLocal(db, thread).statEffects->db);
}

LambdaStatEffectAdapter TableAdapters::getLambdaEffects(GameDB db, size_t thread) {
  return ::getLambdaEffects(getThreadLocal(db, thread).statEffects->db);
}

AreaForceStatEffectAdapter TableAdapters::getAreaForceEffects(GameDB db, size_t thread) {
  return ::getAreaForceEffects(getThreadLocal(db, thread).statEffects->db);
}

GameObjectAdapter TableAdapters::getGameObjects(GameDB db) {
  auto& table = std::get<GameObjectTable>(db.db.mTables);
  return {
    getTransform(table),
    getPhysics(table),
    getStableRow(table)
  };
}

GameObjectAdapter TableAdapters::getGameplayGameObjects(GameDB db) {
  auto& table = std::get<GameObjectTable>(db.db.mTables);
  return {
    getGameplayTransform(table),
    getGameplayPhysics(table),
    getStableRow(table)
  };
}

GameObjectAdapter TableAdapters::getStaticGameObjects(GameDB db) {
  auto& table = std::get<StaticGameObjectTable>(db.db.mTables);
  return {
    getTransform(table),
    PhysicsObjectAdapter{},
    getStableRow(table)
  };
}

GameObjectAdapter TableAdapters::getGameplayStaticGameObjects(GameDB db) {
  auto& table = std::get<StaticGameObjectTable>(db.db.mTables);
  return {
    getGameplayTransform(table),
    PhysicsObjectAdapter{},
    getStableRow(table)
  };
}

GlobalsAdapter TableAdapters::getGlobals(GameDB db) {
  auto& table = std::get<GlobalGameData>(db.db.mTables);
  return {
    &std::get<SharedRow<SceneState>>(table.mRows).at(),
    &std::get<SharedRow<PhysicsTableIds>>(table.mRows).at(),
    &std::get<SharedRow<FileSystem>>(table.mRows).at(),
    &std::get<SharedRow<StableElementMappings>>(table.mRows).at(),
    &std::get<SharedRow<ConstraintsTableMappings>>(table.mRows).at(),
    &std::get<SharedRow<Scheduler>>(table.mRows).at(),
    &std::get<ExternalDatabasesRow>(table.mRows).at(),
    std::get<ThreadLocalsRow>(table.mRows).at().instance.get()
  };
}

PlayerAdapter TableAdapters::getPlayer(GameDB db) {
  auto& table = std::get<PlayerTable>(db.db.mTables);
  return {
    GameObjectAdapter{
      getTransform(table),
      getPhysics(table),
      getStableRow(table),
    },
    &std::get<Row<PlayerInput>>(table.mRows),
    &std::get<Row<PlayerKeyboardInput>>(table.mRows)
  };
}

PlayerAdapter TableAdapters::getGameplayPlayer(GameDB db) {
  auto& table = std::get<PlayerTable>(db.db.mTables);
  return {
    GameObjectAdapter{
      getGameplayTransform(table),
      getGameplayPhysics(table),
      getStableRow(table),
    },
    &std::get<Row<PlayerInput>>(table.mRows),
    &std::get<Row<PlayerKeyboardInput>>(table.mRows)
  };
}

CentralStatEffectAdapter TableAdapters::getCentralStatEffects(GameDB db) {
  auto& stats = getStatEffects(db);
  return {
    ::getPositionEffects(stats.db),
    ::getVelocityEffects(stats.db),
    ::getLambdaEffects(stats.db)
  };
}

size_t TableAdapters::addStatEffectsSharedLifetime(StatEffectBaseAdapter& base, size_t lifetime, const size_t* stableIds, size_t count) {
  const size_t firstIndex = base.modifier.addElements(count);
  for(size_t i = 0; i < count; ++i) {
    base.lifetime->at(i + firstIndex) = lifetime;
    base.owner->at(i + firstIndex) = stableIds ? StableElementID::fromStableID(stableIds[i]) : StableElementID::invalid();
  }
  return firstIndex;
}
