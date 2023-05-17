#pragma once

#include "stat/AreaForceStatEffect.h"
#include "stat/LambdaStatEffect.h"
#include "stat/PositionStatEffect.h"
#include "stat/VelocityStatEffect.h"

namespace AllStatEffects {
  struct Globals {
    StableElementMappings stableMappings;
    DatabaseDescription description;
  };
  struct GlobalRow : SharedRow<Globals> {};
  struct GlobalTable : Table<GlobalRow> {};
}

using StatEffectDatabase = Database<
  AllStatEffects::GlobalTable,
  LambdaStatEffectTable,
  PositionStatEffectTable,
  VelocityStatEffectTable,
  AreaForceStatEffectTable
>;

// To allow forward declarations
struct StatEffectDBOwned {
  StatEffectDatabase db;
};

struct StatEffectDB {
  StatEffectDatabase& db;
};

struct AllStatTasks {
  //Requires mutable access to position and rotation
  TaskRange positionSetters;
  //Requires mutable access to linear and angular velocity
  TaskRange velocitySetters;
  //Read position set velocity
  TaskRange posGetVelSet;
  //Requires exclusive access to the gameplay portion of the database, can add/remove elements
  TaskRange synchronous;
};

namespace StatEffect {
  void initGlobals(StatEffectDatabase& db);
  //Remove all elements of 'from' and put them in 'to'
  //Intended to be used to move newly created thread local effects to the central database
  TaskRange moveTo(StatEffectDatabase& from, StatEffectDatabase& to);
  AllStatTasks createTasks(GameDB db, StatEffectDatabase& centralStats);

  template<class TableT>
  size_t addEffects(size_t countToAdd, TableT& table, StableElementMappings& mappings) {
    size_t startIndex = TableOperations::size(table);
    constexpr UnpackedDatabaseElementID tableId = UnpackedDatabaseElementID::fromPacked(StatEffectDatabase::getTableIndex<TableT>());
    TableOperations::stableResizeTable(table, tableId, startIndex + countToAdd, mappings);

    return startIndex;
  }

  AllStatEffects::Globals& getGlobals(StatEffectDatabase& db);

  template<class TableT>
  size_t addEffects(size_t countToAdd, TableT& table, StatEffectDatabase& db) {
    return addEffects(countToAdd, table, getGlobals(db).stableMappings);
  }
};