#include "Precompile.h"
#include "World.h"

#include "Simulation.h"
#include "TableAdapters.h"
#include "unity.h"

namespace World {
  using namespace Tags;

  TaskRange _enforceWorldBoundary(GameDatabase& db) {
    auto root = TaskNode::createEmpty();
    PROFILE_SCOPE("simulation", "boundary");
    SceneState& scene = std::get<0>(std::get<GlobalGameData>(db.mTables).mRows).at();
    const GameConfig* config = TableAdapters::getConfig({ db }).game;
    const float boundarySpringConstant = config->boundarySpringConstant;
    Queries::viewEachRow(db, [&](FloatRow<GPos, X>& pos, FloatRow<GLinImpulse, X>& linVel) {
      root->mChildren.push_back(TaskNode::create([&pos, &linVel, &scene, config](...) {
        ispc::repelWorldBoundary(pos.mElements.data(), linVel.mElements.data(), scene.mBoundaryMin.x, scene.mBoundaryMax.x, config->boundarySpringConstant, (uint32_t)pos.mElements.size());
      }));
    });
    Queries::viewEachRow(db, [&](FloatRow<GPos, Y>& pos, FloatRow<GLinImpulse, Y>& linVel) {
      root->mChildren.push_back(TaskNode::create([&pos, &linVel, &scene, config](...) {
        ispc::repelWorldBoundary(pos.mElements.data(), linVel.mElements.data(), scene.mBoundaryMin.y, scene.mBoundaryMax.y, config->boundarySpringConstant, (uint32_t)pos.mElements.size());
      }));
    });

    return TaskBuilder::addEndSync(root);
  }

  //Read GPos, GlobalGameData
  //Write GLinImpulse
  TaskRange enforceWorldBoundary(GameDB db) {
    return _enforceWorldBoundary(db.db);
  }
}