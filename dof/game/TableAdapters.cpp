#include "Precompile.h"
#include "TableAdapters.h"

#include "stat/AllStatEffects.h"
#include "Simulation.h"
#include "ThreadLocals.h"
#include "AppBuilder.h"
#include "Narrowphase.h"

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

TransformAdapter TableAdapters::getTransform(RuntimeDatabaseTaskBuilder& task, const TableID& table) {
  TransformAdapter result;
  auto [px, py, rx, ry] = task.query<
    FloatRow<Tags::Pos, Tags::X>, FloatRow<Tags::Pos, Tags::Y>,
    FloatRow<Tags::Rot, Tags::CosAngle>, FloatRow<Tags::Rot, Tags::SinAngle>
  >(table).get(0);
  result.posX = px.get();
  result.posY = py.get();
  result.rotX = rx.get();
  result.rotY = ry.get();
  return result;
}

TransformAdapter TableAdapters::getGameplayTransform(RuntimeDatabaseTaskBuilder& task, const TableID& table) {
  TransformAdapter result;
  auto [px, py, rx, ry] = task.query<
    FloatRow<Tags::GPos, Tags::X>, FloatRow<Tags::GPos, Tags::Y>,
    FloatRow<Tags::GRot, Tags::CosAngle>, FloatRow<Tags::GRot, Tags::SinAngle>
  >(table).get(0);
  result.posX = px.get();
  result.posY = py.get();
  result.rotX = rx.get();
  result.rotY = ry.get();
  return result;
}

PhysicsObjectAdapter TableAdapters::getPhysics(RuntimeDatabaseTaskBuilder& task, const TableID& table) {
  auto q = task.query<
    FloatRow<Tags::LinVel, Tags::X>, FloatRow<Tags::LinVel, Tags::Y>,
    FloatRow<Tags::AngVel, Tags::Angle>,
    FloatRow<Tags::GLinImpulse, Tags::X>, FloatRow<Tags::GLinImpulse, Tags::Y>,
    FloatRow<Tags::GAngImpulse, Tags::Angle>,
    Narrowphase::CollisionMaskRow
  >(table);
  if(q.size()) {
    auto [vx, vy, ax, ix, iy, ia, m] = q.get(0);
    return PhysicsObjectAdapter{
      .linVelX = vx.get(),
      .linVelY = vy.get(),
      .angVel = ax.get(),
      .linImpulseX = ix.get(),
      .linImpulseY = iy.get(),
      .angImpulse = ia.get(),
      .collisionMask = m.get(),
    };
  }
  return {};
}

PhysicsObjectAdapter TableAdapters::getGameplayPhysics(RuntimeDatabaseTaskBuilder& task, const TableID& table) {
  auto q = task.query<
    FloatRow<Tags::GLinVel, Tags::X>, FloatRow<Tags::GLinVel, Tags::Y>,
    FloatRow<Tags::GAngVel, Tags::Angle>,
    FloatRow<Tags::GLinImpulse, Tags::X>, FloatRow<Tags::GLinImpulse, Tags::Y>,
    FloatRow<Tags::GAngImpulse, Tags::Angle>,
    Narrowphase::CollisionMaskRow
  >(table);
  if(q.size()) {
    auto [vx, vy, va, ix, iy, ia, m] = q.get(0);
    return PhysicsObjectAdapter{
      .linVelX = vx.get(),
      .linVelY = vy.get(),
      .angVel = va.get(),
      .linImpulseX = ix.get(),
      .linImpulseY = iy.get(),
      .angImpulse = ia.get(),
      .collisionMask = m.get(),
    };
  }
  return {};
}

GameObjectAdapter TableAdapters::getGameObject(RuntimeDatabaseTaskBuilder& task, const TableID& table) {
  auto t = getTransform(task, table);
  auto p = getPhysics(task, table);
  auto& stable = task.query<const StableIDRow>(table).get<0>(0);
  return { t, p, &stable };
}

GameObjectAdapter TableAdapters::getGameplayGameObject(RuntimeDatabaseTaskBuilder& task, const TableID& table) {
  auto t = getGameplayTransform(task, table);
  auto p = getGameplayPhysics(task, table);
  auto& stable = task.query<const StableIDRow>(table).get<0>(0);
  return { t, p, &stable };
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

const float* TableAdapters::getDeltaTime(RuntimeDatabaseTaskBuilder& task) {
  return &getGameConfig(task)->world.deltaTime;
}
