#include "Precompile.h"
#include "RendererTableAdapters.h"

#include "Renderer.h"

QuadPassAdapter RendererTableAdapters::getQuadPass(QuadPassTable::Type& table) {
  return {
    &std::get<QuadPassTable::PosX>(table.mRows),
    &std::get<QuadPassTable::PosY>(table.mRows),
    &std::get<QuadPassTable::RotX>(table.mRows),
    &std::get<QuadPassTable::RotY>(table.mRows),
    &std::get<QuadPassTable::LinVelX>(table.mRows),
    &std::get<QuadPassTable::LinVelY>(table.mRows),
    &std::get<QuadPassTable::AngVel>(table.mRows),
    &std::get<QuadPassTable::Tint>(table.mRows),
    &std::get<QuadPassTable::IsImmobile>(table.mRows),
    &std::get<QuadPassTable::UV>(table.mRows),
    &std::get<QuadPassTable::Texture>(table.mRows),
    &std::get<QuadPassTable::Pass>(table.mRows)
  };
}

RendererGlobalsAdapter RendererTableAdapters::getGlobals(RuntimeDatabaseTaskBuilder& task) {
  auto q = task.query<Row<OGLState>, Row<WindowData>>();
  return {
    q.tryGetSingletonElement<0>(),
    q.tryGetSingletonElement<1>()
  };
}

RenderDebugAdapter RendererTableAdapters::getDebug(RendererDB db) {
  auto& table = std::get<DebugLinePassTable::Type>(db.db.mTables);
  return {
    &table,
    &std::get<DebugLinePassTable::Points>(table.mRows)
  };
}
