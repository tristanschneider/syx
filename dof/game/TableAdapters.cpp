#include "Precompile.h"
#include "TableAdapters.h"

#include "stat/AllStatEffects.h"
#include "Simulation.h"
#include "ThreadLocals.h"

ConfigAdapter TableAdapters::getConfig(GameDB db) {
  auto& c = std::get<GlobalGameData>(db.db.mTables);
  Config::GameConfig& g = std::get<SharedRow<Config::GameConfig>>(c.mRows).at();
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
      &std::get<StatEffect::Continuations>(table.mRows),
      StableTableModifierInstance::get<StatEffectDatabase>(table, StatEffect::getGlobals(db).stableMappings),
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
      TableOperations::tryGetRow<Tint>(table)
    };
  }

  template<class TableT>
  StableIDRow* getStableRow(TableT& table) {
    return TableOperations::tryGetRow<StableIDRow>(table);
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
      &std::get<AreaForceStatEffect::CommandRow>(table.mRows)
    };
  }

  FollowTargetByPositionStatEffectAdapter getFollowTargetByPositionEffects(StatEffectDatabase& db) {
    auto& table = std::get<FollowTargetByPositionStatEffectTable>(db.mTables);
    return {
      getStatBase(table, db),
      &std::get<FollowTargetByPositionStatEffect::CommandRow>(table.mRows)
    };
  }

  FollowTargetByVelocityStatEffectAdapter getFollowTargetByVelocityEffects(StatEffectDatabase& db) {
    auto& table = std::get<FollowTargetByVelocityStatEffectTable>(db.mTables);
    return {
      getStatBase(table, db),
      &std::get<FollowTargetByVelocityStatEffect::CommandRow>(table.mRows)
    };
  }


  DamageStatEffectAdapter getDamageEffects(StatEffectDatabase& db) {
    auto& table = std::get<DamageStatEffectTable>(db.mTables);
    return {
      getStatBase(table, db),
      &std::get<DamageStatEffect::CommandRow>(table.mRows)
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

FollowTargetByPositionStatEffectAdapter TableAdapters::getFollowTargetByPositionEffects(GameDB db, size_t thread) {
  return ::getFollowTargetByPositionEffects(getThreadLocal(db, thread).statEffects->db);
}

FollowTargetByVelocityStatEffectAdapter TableAdapters::getFollowTargetByVelocityEffects(GameDB db, size_t thread) {
  return ::getFollowTargetByVelocityEffects(getThreadLocal(db, thread).statEffects->db);
}

DamageStatEffectAdapter TableAdapters::getDamageEffects(GameDB db, size_t thread) {
  return ::getDamageEffects(getThreadLocal(db, thread).statEffects->db);
}

GameObjectAdapter TableAdapters::getGameplayObjectInTable(GameDB db, size_t tableIndex) {
  GameObjectAdapter result;
  db.db.visitOneByIndex(GameDatabase::ElementID{ tableIndex, 0 }, [&result](auto& table) {
    result = GameObjectAdapter {
      getGameplayTransform(table),
      getGameplayPhysics(table),
      getStableRow(table)
    };
  });
  return result;
}

GameObjectAdapter TableAdapters::getObjectInTable(GameDB db, size_t tableIndex) {
  GameObjectAdapter result;
  db.db.visitOneByIndex(GameDatabase::ElementID{ tableIndex, 0 }, [&result](auto& table) {
    result = GameObjectAdapter {
      getTransform(table),
      getPhysics(table),
      getStableRow(table)
    };
  });
  return result;
}

GameObjectAdapter TableAdapters::getGameObjects(GameDB db) {
  auto& table = std::get<GameObjectTable>(db.db.mTables);
  return {
    getTransform(table),
    getPhysics(table),
    getStableRow(table)
  };
}

FragmentAdapter TableAdapters::getFragments(GameDB db) {
  auto& table = std::get<GameObjectTable>(db.db.mTables);
  return getFragment(table);
}

FragmentAdapter TableAdapters::getFragmentsInTable(GameDB db, size_t tableIndex) {
  FragmentAdapter result;
  db.db.visitOneByIndex(GameDatabase::ElementID{ tableIndex, 0 }, [&result](auto& table) {
    result = getFragment(table);
  });
  return result;
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

CameraAdapater TableAdapters::getCamera(GameDB db) {
  auto& table = std::get<CameraTable>(db.db.mTables);
  return {
    GameObjectAdapter {
      getTransform(table),
      {},
      getStableRow(table)
    }
  };
}

TargetPosAdapter TableAdapters::getTargetPos(GameDB db) {
  auto& table = std::get<TargetPosTable>(db.db.mTables);
  return {
    &std::get<FloatRow<Tags::Pos, Tags::X>>(table.mRows),
    &std::get<FloatRow<Tags::Pos, Tags::Y>>(table.mRows),
    &std::get<StableIDRow>(table.mRows),
    StableTableModifierInstance::get<GameDatabase>(table, getStableMappings(db))
  };
}

SpatialQueryAdapter TableAdapters::getSpatialQueries(GameDB db) {
  auto& table = std::get<SpatialQuery::SpatialQueriesTable>(db.db.mTables);
  return {
    &std::get<SpatialQuery::Gameplay<SpatialQuery::QueryRow>>(table.mRows),
    &std::get<SpatialQuery::Gameplay<SpatialQuery::ResultRow>>(table.mRows),
    &std::get<SpatialQuery::Gameplay<SpatialQuery::GlobalsRow>>(table.mRows),
    getStableRow(table),
    &getStableMappings(db),
    &std::get<SpatialQuery::Gameplay<SpatialQuery::NeedsResubmitRow>>(table.mRows),
    &std::get<SpatialQuery::Gameplay<SpatialQuery::LifetimeRow>>(table.mRows)
  };
}

CentralStatEffectAdapter TableAdapters::getCentralStatEffects(GameDB db) {
  auto& stats = getStatEffects(db);
  return {
    ::getPositionEffects(stats.db),
    ::getVelocityEffects(stats.db),
    ::getLambdaEffects(stats.db),
    ::getFollowTargetByPositionEffects(stats.db),
    ::getFollowTargetByVelocityEffects(stats.db)
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

DebugLineAdapter TableAdapters::getDebugLines(GameDB db) {
  auto& debug = std::get<DebugLineTable>(db.db.mTables);
  return {
    &std::get<Row<DebugPoint>>(debug.mRows),
    TableModifierInstance::get(debug)
  };
}
