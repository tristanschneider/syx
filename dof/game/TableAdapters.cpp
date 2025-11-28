#include "Precompile.h"
#include "TableAdapters.h"

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

size_t TableAdapters::getThreadCount(RuntimeDatabaseTaskBuilder& task) {
  return task.query<const ThreadLocalsRow>().tryGetSingletonElement()->instance->getThreadCount();
}

DebugLineAdapter TableAdapters::getDebugLines(RuntimeDatabaseTaskBuilder& task) {
  auto query = task.query<Row<DebugPoint>>();
  DebugLineAdapter result;
  if(query.size()) {
    result.points = &query.get<0>(0);
    result.pointModifier = task.getModifierForTable(query[0]);
    if(auto q = task.query<Row<DebugText>>(); q.size()) {
      result.text = &q.get<0>(0);
      result.textModifier = task.getModifierForTable(q[0]);
    }
  }
  return result;
}
