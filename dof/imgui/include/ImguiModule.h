#pragma once

class RuntimeDatabaseTaskBuilder;
struct StableElementMappings;
class IAppBuilder;
struct IDatabase;

namespace ImguiModule {
  //Used for modules to query if imgui is ready to accept commands
  const bool* queryIsEnabled(RuntimeDatabaseTaskBuilder& task);

  std::unique_ptr<IDatabase> createDatabase(RuntimeDatabaseTaskBuilder&& builder, StableElementMappings& mappings);

  void update(IAppBuilder& builder);
};