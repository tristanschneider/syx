#pragma once

#include "DatabaseID.h"

struct IDatabase;
struct StableElementMappings;
class IAppBuilder;
class RuntimeDatabaseTaskBuilder;
struct RuntimeDatabaseArgs;

namespace GameDatabase {
  struct Tables {
    Tables(RuntimeDatabaseTaskBuilder& task);

    TableID player;
    TableID terrain;
    TableID activeFragment;
    TableID completedFragment;
    TableID physicsObjsWithZ;
    TableID fragmentSpawner;
  };

  void create(RuntimeDatabaseArgs& args);
  void configureDefaults(IAppBuilder& builder);
}