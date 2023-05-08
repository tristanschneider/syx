#include "Precompile.h"
#include "TableAdapters.h"

#include "stat/AllStatEffects.h"
#include "Simulation.h"
#include "ThreadLocals.h"

ConfigAdapter TableAdapters::getConfig(GameDB db) {
  auto& c = std::get<ConfigTable>(db.db.mTables);
  return {
    &std::get<SharedRow<DebugConfig>>(c.mRows).at(),
    &std::get<SharedRow<PhysicsConfig>>(c.mRows).at(),
    &std::get<SharedRow<GameConfig>>(c.mRows).at(),
    &std::get<SharedRow<GraphicsConfig>>(c.mRows).at()
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
  StatEffectBaseAdapter getStatBase(TableT& table) {
    return {
      &std::get<StatEffect::Owner>(table.mRows),
      &std::get<StatEffect::Lifetime>(table.mRows),
      &std::get<StatEffect::Global>(table.mRows)
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
  PhysicsObjectAdapter getPhysics(TableT& table) {
    return {
      &std::get<FloatRow<Tags::LinVel, Tags::X>>(table.mRows),
      &std::get<FloatRow<Tags::LinVel, Tags::Y>>(table.mRows),
      &std::get<FloatRow<Tags::AngVel, Tags::Angle>>(table.mRows)
    };
  }

  PositionStatEffectAdapter getPositionEffects(StatEffectDatabase& db) {
    auto& table = std::get<PositionStatEffectTable>(db.mTables);
    return {
      getStatBase(table),
      &std::get<PositionStatEffect::CommandRow>(table.mRows),
    };
  }

  VelocityStatEffectAdapter getVelocityEffects(StatEffectDatabase& db) {
    auto& table = std::get<VelocityStatEffectTable>(db.mTables);
    return {
      getStatBase(table),
      &std::get<VelocityStatEffect::CommandRow>(table.mRows),
    };
  }

  LambdaStatEffectAdapter getLambdaEffects(StatEffectDatabase& db) {
    auto& table = std::get<LambdaStatEffectTable>(db.mTables);
    return {
      getStatBase(table),
      &std::get<LambdaStatEffect::LambdaRow>(table.mRows),
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

GameObjectAdapter TableAdapters::getGameObjects(GameDB db) {
  auto& table = std::get<GameObjectTable>(db.db.mTables);
  return {
    getTransform(table),
    getPhysics(table),
    &std::get<StableIDRow>(table.mRows)
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

CentralStatEffectAdapter TableAdapters::getCentralStatEffects(GameDB db) {
  auto& stats = getStatEffects(db);
  return {
    ::getPositionEffects(stats.db),
    ::getVelocityEffects(stats.db),
    ::getLambdaEffects(stats.db)
  };
}
