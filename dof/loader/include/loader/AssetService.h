#pragma once

class IAppBuilder;
struct IDatabase;
struct StableElementMappings;

namespace Loader {
  void createDB(RuntimeDatabaseArgs& args);
  void processRequests(IAppBuilder& builder);
}
