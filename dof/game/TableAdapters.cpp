#include "Precompile.h"
#include "TableAdapters.h"

#include "Simulation.h"

ConfigAdapter TableAdapters::getConfig(GameDB db) {
  auto& c = std::get<ConfigTable>(db.db.mTables);
  return {
    &std::get<SharedRow<DebugConfig>>(c.mRows).at(),
    &std::get<SharedRow<PhysicsConfig>>(c.mRows).at(),
    &std::get<SharedRow<GameConfig>>(c.mRows).at(),
    &std::get<SharedRow<GraphicsConfig>>(c.mRows).at()
  };
}
