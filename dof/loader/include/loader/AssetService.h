#pragma once

class IAppBuilder;
struct IDatabase;
struct StableElementMappings;
class ElementRef;
struct AppTaskArgs;
struct RuntimeDatabaseArgs;

namespace Loader {
  void createDB(RuntimeDatabaseArgs& args);
  void processRequests(IAppBuilder& builder);
}
