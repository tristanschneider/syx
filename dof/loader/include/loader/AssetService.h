#pragma once

class IAppBuilder;
struct IDatabase;
struct StableElementMappings;

namespace Loader {
  struct SceneAsset;

  std::unique_ptr<IDatabase> createDB(StableElementMappings& mappings);
  void processRequests(IAppBuilder& builder);


  SceneAsset hack();
}
