#pragma once

#include "Renderer.h"
#include "Simulation.h"

namespace ImguiModule {
  //Used for modules to query if imgui is ready to accept commands
  const bool* queryIsEnabled(RuntimeDatabaseTaskBuilder& task);

  std::unique_ptr<IDatabase> createDatabase(RuntimeDatabaseTaskBuilder&& builder);

  void update(IAppBuilder& builder);
};