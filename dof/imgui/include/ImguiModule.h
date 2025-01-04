#pragma once

class RuntimeDatabaseTaskBuilder;
struct StableElementMappings;
class IAppBuilder;
struct IDatabase;
struct RuntimeDatabaseArgs;
class IAppModule;

namespace ImguiModule {
  //Used for modules to query if imgui is ready to accept commands
  const bool* queryIsEnabled(RuntimeDatabaseTaskBuilder& task);

  std::unique_ptr<IAppModule> createModule();
};