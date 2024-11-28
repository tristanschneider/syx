#pragma once

#include "DatabaseID.h"

struct IDatabase;
struct StableElementMappings;
class IAppBuilder;
class RuntimeDatabaseTaskBuilder;

namespace GameDatabase {
  struct Tables {
    Tables(RuntimeDatabaseTaskBuilder& task);

    TableID player;
    TableID terrain;
    TableID activeFragment;
    TableID completedFragment;
  };

  std::unique_ptr<IDatabase> create(StableElementMappings& mappings);
  void configureDefaults(IAppBuilder& builder);
}