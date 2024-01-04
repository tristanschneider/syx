#pragma once

struct IDatabase;
struct StableElementMappings;
class IAppBuilder;

namespace GameDatabase {
  std::unique_ptr<IDatabase> create(StableElementMappings& mappings);
  void configureDefaults(IAppBuilder& builder);
}