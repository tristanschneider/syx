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
}

PositionStatEffectAdapter TableAdapters::getPositionEffects(GameDB db, size_t thread) {
  auto& table = std::get<PositionStatEffectTable>(getThreadLocal(db, thread).statEffects->db.mTables);
  return {
    getStatBase(table),
    &std::get<PositionStatEffect::CommandRow>(table.mRows),
  };
}

VelocityStatEffectAdapter TableAdapters::getVelocityEffects(GameDB db, size_t thread) {
  auto& table = std::get<VelocityStatEffectTable>(getThreadLocal(db, thread).statEffects->db.mTables);
  return {
    getStatBase(table),
    &std::get<VelocityStatEffect::CommandRow>(table.mRows),
  };

}

LambdaStatEffectAdapter TableAdapters::getLambdaEffects(GameDB db, size_t thread) {
  auto& table = std::get<LambdaStatEffectTable>(getThreadLocal(db, thread).statEffects->db.mTables);
  return {
    getStatBase(table),
    &std::get<LambdaStatEffect::LambdaRow>(table.mRows),
  };
}

GameObjectAdapter TableAdapters::getGameObjects(GameDB db) {
  auto& table = std::get<GameObjectTable>(db.db.mTables);
  return {
    getTransform(table),
    getPhysics(table),
    &std::get<StableIDRow>(table.mRows)
  };
}
