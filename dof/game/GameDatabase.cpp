#include "Precompile.h"
#include "GameDatabase.h"

#include "RuntimeDatabase.h"

//TODO: move GameDatabase into this file
#include "Simulation.h"

namespace GameData {
  RuntimeDatabaseArgs reflectDB(GameDatabase& db) {
    RuntimeDatabaseArgs result;
    DBReflect::reflect(db, result, std::get<SharedRow<StableElementMappings>>(std::get<GlobalGameData>(db.mTables).mRows).at());
    return result;
  }

  struct Impl : IDatabase {
    Impl()
      : runtime(reflectDB(db)) {
    }

    RuntimeDatabase& getRuntime() override {
      return runtime;
    }

    GameDatabase db;
    RuntimeDatabase runtime;
  };

  std::unique_ptr<IDatabase> create() {
    return std::make_unique<Impl>();
  }
}
