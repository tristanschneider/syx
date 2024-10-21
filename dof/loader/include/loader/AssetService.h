#pragma once

class IAppBuilder;
struct IDatabase;
struct StableElementMappings;

namespace Loader {
  std::unique_ptr<IDatabase> createDB(StableElementMappings& mappings);
  void processRequests(IAppBuilder& builder);
}
