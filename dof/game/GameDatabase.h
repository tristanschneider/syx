#pragma once

#include "DatabaseID.h"

struct IDatabase;
struct StableElementMappings;
class IAppBuilder;
class RuntimeDatabaseTaskBuilder;
struct RuntimeDatabaseArgs;
class RuntimeDatabase;
class StorageTableBuilder;
struct AppTaskArgs;

namespace GameDatabase {
  struct Tables {
    Tables() = default;
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

  struct RenderableOptions {
    //All objects in this table use the same texture
    bool sharedTexture = true;
    bool sharedMesh = true;
  };

  StorageTableBuilder& addVelocity25D(StorageTableBuilder& table);
  StorageTableBuilder& addGameplayCopy(StorageTableBuilder& table);
  StorageTableBuilder& addRenderable(StorageTableBuilder& table, const RenderableOptions& ops);

  void create(RuntimeDatabaseArgs& args);
  void configureDefaults(IAppBuilder& builder);
}