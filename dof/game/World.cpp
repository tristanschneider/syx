#include "Precompile.h"
#include "World.h"

#include "GameMath.h"
#include "Simulation.h"
#include "TableAdapters.h"
#include "unity.h"

#include <cassert>
#include "glm/gtx/norm.hpp"

namespace World {
  using namespace Tags;

  TaskRange _enforceWorldBoundary(GameDatabase& db) {
    auto root = TaskNode::createEmpty();
    PROFILE_SCOPE("simulation", "boundary");
    SceneState& scene = std::get<0>(std::get<GlobalGameData>(db.mTables).mRows).at();
    const Config::GameConfig* config = TableAdapters::getConfig({ db }).game;
    Queries::viewEachRow(db, [&](FloatRow<GPos, X>& pos, FloatRow<GLinImpulse, X>& linVel) {
      root->mChildren.push_back(TaskNode::create([&pos, &linVel, &scene, config](...) {
        ispc::repelWorldBoundary(pos.mElements.data(), linVel.mElements.data(), scene.mBoundaryMin.x, scene.mBoundaryMax.x, config->world.boundarySpringConstant, (uint32_t)pos.mElements.size());
      }));
    });
    Queries::viewEachRow(db, [&](FloatRow<GPos, Y>& pos, FloatRow<GLinImpulse, Y>& linVel) {
      root->mChildren.push_back(TaskNode::create([&pos, &linVel, &scene, config](...) {
        ispc::repelWorldBoundary(pos.mElements.data(), linVel.mElements.data(), scene.mBoundaryMin.y, scene.mBoundaryMax.y, config->world.boundarySpringConstant, (uint32_t)pos.mElements.size());
      }));
    });
    //Once x and y are done, see if either of them changed to update the flag
    auto sync = TaskNode::createEmpty();
    Queries::viewEachRow(db, [&](const FloatRow<GLinImpulse, X>& impulseX,
      const FloatRow<GLinImpulse, Y>& impulseY,
      FragmentFlagsRow& flags,
      FragmentStateMachine::StateRow& state) {
      sync->mChildren.push_back(TaskNode::create([&](enki::TaskSetPartition, uint32_t) {
        //Add follow target at goal to rocket towards goal
        //Change tint, disable collision
        //Add damaging status
        for(size_t i = 0; i < impulseX.size(); ++i) {
          FragmentFlags& flag = flags.at(i);
          //TODO: This is not true, player abilities update before this and would set them
          //Assume that if the impulse changed at all it means it happend from the world boundary
          constexpr float threshold = 0.01f;
          if(glm::length2(TableAdapters::read(i, impulseX, impulseY)) > threshold) {
            //If it was previously in bounds and is now going out, apply the effect
            if(Math::enumCast(flag) & Math::enumCast(FragmentFlags::InBounds)) {
              //Clear flag
              flag = (FragmentFlags)(Math::enumCast(flag) & ~Math::enumCast(FragmentFlags::InBounds));
              //Enqueue the damage effect
              FragmentStateMachine::setState(state.at(i), { FragmentStateMachine::SeekHome{} });
            }
          }
          else {
            //No impulse applied, must be in bounds, set the flag
            flag = (FragmentFlags)(Math::enumCast(flag) | Math::enumCast(FragmentFlags::InBounds));
          }
        }
      }));
    });

    TaskBuilder::_addSyncDependency(*root, sync);

    return TaskBuilder::addEndSync(root);
  }

  //Read GPos, GlobalGameData
  //Write GLinImpulse
  TaskRange enforceWorldBoundary(GameDB db) {
    return _enforceWorldBoundary(db.db);
  }
}