#include "Precompile.h"
#include "World.h"

#include "Simulation.h"
#include "TableAdapters.h"
#include "unity.h"

namespace World {
  using namespace Tags;

  void _enforceWorldBoundary(GameDatabase& db) {
    PROFILE_SCOPE("simulation", "boundary");
    SceneState& scene = std::get<0>(std::get<GlobalGameData>(db.mTables).mRows).at();
    const glm::vec2 boundaryMin = scene.mBoundaryMin;
    const glm::vec2 boundaryMax = scene.mBoundaryMax;
    const GameConfig* config = TableAdapters::getConfig({ db }).game;
    const float boundarySpringConstant = config->boundarySpringConstant;
    Queries::viewEachRow<FloatRow<Pos, X>,
      FloatRow<LinVel, X>>(db,
        [&](FloatRow<Pos, X>& pos, FloatRow<LinVel, X>& linVel) {
      ispc::repelWorldBoundary(pos.mElements.data(), linVel.mElements.data(), boundaryMin.x, boundaryMax.x, boundarySpringConstant, (uint32_t)pos.mElements.size());
    });
    Queries::viewEachRow<FloatRow<Pos, Y>,
      FloatRow<LinVel, Y>>(db,
        [&](FloatRow<Pos, Y>& pos, FloatRow<LinVel, Y>& linVel) {
      ispc::repelWorldBoundary(pos.mElements.data(), linVel.mElements.data(), boundaryMin.y, boundaryMax.y, boundarySpringConstant, (uint32_t)pos.mElements.size());
    });
  }

  void enforceWorldBoundary(GameDB db) {
    _enforceWorldBoundary(db.db);
  }
}