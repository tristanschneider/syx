#pragma once

#include "DatabaseID.h"

struct IDatabase;
struct StableElementMappings;
class IAppBuilder;
class RuntimeDatabaseTaskBuilder;
struct RuntimeDatabaseArgs;
class RuntimeDatabase;
struct AppTaskArgs;

namespace GameDatabase {
  struct Tables {
    Tables(RuntimeDatabaseTaskBuilder& task);
    Tables(RuntimeDatabase& db);
    Tables(AppTaskArgs& args);

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