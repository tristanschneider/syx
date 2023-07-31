#include "Precompile.h"
#include "World.h"

#include "GameMath.h"
#include "Simulation.h"
#include "TableAdapters.h"
#include "unity.h"

#include "stat/LambdaStatEffect.h"
#include "stat/FollowTargetByVelocityEffect.h"
#include <cassert>

namespace World {
  using namespace Tags;

  //TODO: should have a generic stat effect for queueing removal
  //Target is a simplified case that can be done manually for now
  void queueRemoveTarget(GameDB db, const StableElementID& target, size_t thread) {
    LambdaStatEffectAdapter lambda = TableAdapters::getLambdaEffects(db, thread);
    size_t effect = TableAdapters::addStatEffectsSharedLifetime(lambda.base, StatEffect::INSTANT, &target.mStableID, 1);
    lambda.command->at(effect) = [](LambdaStatEffect::Args& args) {
      if(args.resolvedID != StableElementID::invalid()) {
        assert(args.resolvedID.toPacked<GameDatabase>().getTableIndex() == GameDatabase::getTableIndex<TargetPosTable>().getTableIndex());
        TableOperations::stableSwapRemove(std::get<TargetPosTable>(args.db->db.mTables), args.resolvedID.toPacked<GameDatabase>(), TableAdapters::getStableMappings(*args.db));
      }
    };
  }

  void addDamageStatEffect(GameDB db, const StableElementID& self, FollowTargetByVelocityStatEffectAdapter follow) {
    //Create a target to use for the follow effect
    TargetPosAdapter targets = TableAdapters::getTargetPos(db);
    const size_t newTarget = targets.modifier.addElements(1);
    StableElementID stableTarget = StableElementID::fromStableRow(newTarget, *targets.stable);

    const size_t followTime = 100;
    const float springConstant = 0.001f;

    //Create the follow effect and point it at the target
    const size_t followEffect = TableAdapters::addStatEffectsSharedLifetime(follow.base, followTime, &self.mStableID, 1);
    follow.command->at(followEffect).mode = FollowTargetByVelocityStatEffect::SpringFollow{ springConstant };
    follow.base.target->at(followEffect) = stableTarget;

    //Add the damage effect which affects tints, damages or repels nearby targets fragments, and disables collision
    //TODO:

    //Add a continuation to remove everything once it's complete
    follow.base.continuations->at(followEffect).onComplete.push_back([stableTarget](StatEffect::Continuation::Args& args) {
      queueRemoveTarget(args.db, stableTarget, args.thread);
    });
  }

  void queueDamageEffect(size_t i, const StableIDRow& stable, LambdaStatEffectAdapter lambda) {
    const size_t effect = TableAdapters::addStatEffectsSharedLifetime(lambda.base, StatEffect::INSTANT, &stable.at(i), 1);
    bool called = false;
    lambda.command->at(effect) = [called](LambdaStatEffect::Args& args) mutable {
      //Since this is in a lambda callback it's the main thread so use the central stat effects instead of the thread local ones
      FollowTargetByVelocityStatEffectAdapter follow = TableAdapters::getCentralStatEffects(*args.db).followTargetByVelocity;
      addDamageStatEffect(*args.db, args.resolvedID, follow);
    };
  }

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
      const StableIDRow& stableIds) {
      sync->mChildren.push_back(TaskNode::create([&](enki::TaskSetPartition, uint32_t thread) {
        //Add follow target at goal to rocket towards goal
        //Change tint, disable collision
        //Add damaging status
        for(size_t i = 0; i < impulseX.size(); ++i) {
          FragmentFlags& flag = flags.at(i);
          //Assume that if the impulse changed at all it means it happend from the world boundary
          if(impulseX.at(i) || impulseY.at(i)) {
            //If it was previously in bounds and is now going out, apply the effect
            if(Math::enumCast(flag) & Math::enumCast(FragmentFlags::InBounds)) {
              //Clear flag
              flag = (FragmentFlags)(Math::enumCast(flag) & ~Math::enumCast(FragmentFlags::InBounds));
              //Enqueue the effect. This will add a callback to the thread local queue that will eventually apply the damage effect to the fragment for a while
              queueDamageEffect(i, stableIds, TableAdapters::getLambdaEffects({ db }, thread));
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