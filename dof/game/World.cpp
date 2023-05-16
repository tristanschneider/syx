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
    Queries::viewEachRow(db, [&](FloatRow<GPos, X>& pos, FloatRow<GLinImpulse, X>& linVel) {
      ispc::repelWorldBoundary(pos.mElements.data(), linVel.mElements.data(), boundaryMin.x, boundaryMax.x, boundarySpringConstant, (uint32_t)pos.mElements.size());
    });
    Queries::viewEachRow(db, [&](FloatRow<GPos, Y>& pos, FloatRow<GLinImpulse, Y>& linVel) {
      ispc::repelWorldBoundary(pos.mElements.data(), linVel.mElements.data(), boundaryMin.y, boundaryMax.y, boundarySpringConstant, (uint32_t)pos.mElements.size());
    });
  }

  //Read GPos, GlobalGameData
  //Write GLinImpulse
  void enforceWorldBoundary(GameDB db) {
    _enforceWorldBoundary(db.db);
  }
}