#include "Precompile.h"
#include "World.h"

#include "AppBuilder.h"
#include "GameMath.h"
#include "Simulation.h"
#include "TableAdapters.h"
#include "unity.h"

#include <cassert>
#include "glm/gtx/norm.hpp"

namespace World {
  using namespace Tags;

  using FloatAlias = QueryAlias<Row<float>>;

  void repelBoundaryAxis(IAppBuilder& builder, const UnpackedDatabaseElementID& table, FloatAlias pos, FloatAlias linVel, size_t axis) {
    auto task = builder.createTask();
    const SceneState* scene = task.query<const SharedRow<SceneState>>().tryGetSingletonElement();
    const Config::WorldConfig* config = &TableAdapters::getGameConfig(task)->world;
    auto query = task.queryAlias(table, pos.read(), linVel);
    task.setName("repel boundary axis");
    task.setCallback([scene, query, axis, config](AppTaskArgs&) mutable {
      auto&& [pos, vel] = query.get(0);
      ispc::repelWorldBoundary(pos->data(), vel->data(), scene->mBoundaryMin[axis], scene->mBoundaryMax[axis], config->boundarySpringConstant, (uint32_t)pos->size());
    });
    builder.submitTask(std::move(task));
  }

  void flagOutOfBounds(IAppBuilder& builder, const UnpackedDatabaseElementID& table) {
    auto task = builder.createTask();
    task.setName("fragment out of bounds");
    auto query = task.query<
      const FloatRow<GLinImpulse, X>, const FloatRow<GLinImpulse, Y>,
      FragmentFlagsRow,
      FragmentStateMachine::StateRow
    >(table);

    task.setCallback([query](AppTaskArgs&) mutable {
      auto&& [impulseX, impulseY, flags, state] = query.get(0);

      //Add follow target at goal to rocket towards goal
      //Change tint, disable collision
      //Add damaging status
      for(size_t i = 0; i < impulseX->size(); ++i) {
        FragmentFlags& flag = flags->at(i);
        //TODO: This is not true, player abilities update before this and would set them
        //Assume that if the impulse changed at all it means it happend from the world boundary
        constexpr float threshold = 0.01f;
        if(glm::length2(TableAdapters::read(i, *impulseX, *impulseY)) > threshold) {
          //If it was previously in bounds and is now going out, apply the effect
          if(Math::enumCast(flag) & Math::enumCast(FragmentFlags::InBounds)) {
            //Clear flag
            flag = (FragmentFlags)(Math::enumCast(flag) & ~Math::enumCast(FragmentFlags::InBounds));
            //Enqueue the damage effect
            FragmentStateMachine::setState(state->at(i), { FragmentStateMachine::SeekHome{} });
          }
        }
        else {
          //No impulse applied, must be in bounds, set the flag
          flag = (FragmentFlags)(Math::enumCast(flag) | Math::enumCast(FragmentFlags::InBounds));
        }
      }
    });

    builder.submitTask(std::move(task));
  }

  void enforceWorldBoundary(IAppBuilder& builder) {
    //auto task= 
    //const SceneState* scene =  const SharedRow<SceneState>
    auto tables = builder.queryTables<
      FloatRow<GPos, X>, FloatRow<GPos, Y>,
      FloatRow<GLinImpulse, X>, FloatRow<GLinImpulse, Y>
    >();

    for(const UnpackedDatabaseElementID& table : tables.matchingTableIDs) {
      repelBoundaryAxis(builder, table, FloatAlias::create<FloatRow<GPos, X>>(), FloatAlias::create<FloatRow<GLinVel, X>>(), 0);
      repelBoundaryAxis(builder, table, FloatAlias::create<FloatRow<GPos, Y>>(), FloatAlias::create<FloatRow<GLinVel, Y>>(), 1);
    }

    auto fragmentTables = builder.queryTables<
      FloatRow<GLinImpulse, X>,
      FloatRow<GLinImpulse, Y>,
      FragmentFlagsRow,
      FragmentStateMachine::StateRow
    >();
    //Once x and y are done, see if either of them changed to update the flag
    for(const UnpackedDatabaseElementID& table : fragmentTables.matchingTableIDs) {
      flagOutOfBounds(builder, table);
    }
  }
}