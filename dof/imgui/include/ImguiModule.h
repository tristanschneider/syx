#pragma once

class RuntimeDatabaseTaskBuilder;
struct StableElementMappings;
class IAppBuilder;
struct IDatabase;
struct RuntimeDatabaseArgs;

namespace ImguiModule {
  //Used for modules to query if imgui is ready to accept commands
  const bool* queryIsEnabled(RuntimeDatabaseTaskBuilder& task);

  void createDatabase(RuntimeDatabaseArgs& args);

  void update(IAppBuilder& builder);
};