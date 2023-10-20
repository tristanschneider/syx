#include "Precompile.h"
#include "GameDatabase.h"

#include "RuntimeDatabase.h"

//TODO: move GameDatabase into this file
#include "Simulation.h"
#include "stat/AllStatEffects.h"

namespace GameData {
  std::unique_ptr<IDatabase> create(StableElementMappings& mappings) {
    return DBReflect::merge(
      DBReflect::createDatabase<GameDatabase>(mappings),
      DBReflect::createDatabase<StatEffectDatabase>(mappings)
    );
  }
}
