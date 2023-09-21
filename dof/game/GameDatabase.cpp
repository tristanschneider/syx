#include "Precompile.h"
#include "GameDatabase.h"

#include "RuntimeDatabase.h"

//TODO: move GameDatabase into this file
#include "Simulation.h"

namespace GameData {
  RuntimeDatabaseArgs reflectDB(GameDatabase& db, StableElementMappings& mappings) {
    RuntimeDatabaseArgs result;
    DBReflect::reflect(db, result, mappings);
    return result;
  }

  struct Impl : IDatabase {
    Impl(StableElementMappings& mappings)
      : runtime(reflectDB(db, mappings)) {
    }

    RuntimeDatabase& getRuntime() override {
      return runtime;
    }

    GameDatabase db;
    RuntimeDatabase runtime;
  };

  std::unique_ptr<IDatabase> create(StableElementMappings& mappings) {
    return std::make_unique<Impl>(mappings);
  }
}
