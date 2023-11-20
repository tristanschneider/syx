#include "Precompile.h"
#include "RendererTableAdapters.h"

#include "Renderer.h"

RendererGlobalsAdapter RendererTableAdapters::getGlobals(RuntimeDatabaseTaskBuilder& task) {
  auto q = task.query<Row<OGLState>, Row<WindowData>>();
  return {
    q.tryGetSingletonElement<0>(),
    q.tryGetSingletonElement<1>()
  };
}
