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

RendererGlobalsAdapter RendererTableAdapters::getGlobals(RendererDB db) {
  auto& table = std::get<GraphicsContext>(db.db.mTables);
  return {
    std::get<Row<OGLState>>(table.mRows).mElements.data(),
    std::get<Row<WindowData>>(table.mRows).mElements.data(),
    TableOperations::size(table)
  };
}

RenderDebugAdapter RendererTableAdapters::getDebug(RendererDB db) {
  auto& table = std::get<DebugLinePassTable::Type>(db.db.mTables);
  return {
    &table,
    &std::get<DebugLinePassTable::Points>(table.mRows)
  };
}
