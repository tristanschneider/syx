#include "Precompile.h"
#include "Fragment.h"

#include <random>
#include "PhysicsSimulation.h"
#include "Simulation.h"
#include "TableAdapters.h"
#include "unity.h"

#include "glm/gtx/norm.hpp"
#include "stat/AllStatEffects.h"
#include "AppBuilder.h"
#include "Narrowphase.h"

namespace Fragment {
  using namespace Tags;

  void _migrateCompletedFragments(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("Migrate completed fragments");
    auto query = task.query<
      const FragmentGoalFoundRow,
      const StableIDRow
    >();
    const UnpackedDatabaseElementID completedTable = builder.queryTables<FragmentGoalFoundTableTag>().matchingTableIDs[0];

    task.setCallback([query, completedTable](AppTaskArgs& args) mutable {
      Events::MovePublisher moveElement{{ &args }};
      for(size_t t = 0; t < query.size(); ++t) {
        auto&& [goalFound, stableRow] = query.get(t);
        for(size_t i = 0; i < goalFound->size(); ++i) {
          //If the goal is found, enqueue a move request to the completed fragments table
          if(goalFound->at(i)) {
            moveElement(StableElementID::fromStableRow(i, *stableRow), completedTable);
          }
        }
      }
    });

    builder.submitTask(std::move(task));
  }

  //Migration enqueues an event to request that table service moves the element to the given table
  //This event triggers just before the table service runs to prepare the state of the object for migration
  //It is not done when the event is queued because other forces could change the object before then and when the migration happens
  void _prepareFragmentMigration(const DBEvents& events, ITableResolver& resolver, IIDResolver& ids) {
    CachedRow<FloatRow<Tags::Pos, Tags::X>> posX;
    CachedRow<FloatRow<Tags::Pos, Tags::Y>> posY;
    CachedRow<const FloatRow<Tags::FragmentGoal, Tags::X>> goalX;
    CachedRow<const FloatRow<Tags::FragmentGoal, Tags::Y>> goalY;
    CachedRow<FloatRow<Tags::Rot, Tags::CosAngle>> rotX;
    CachedRow<FloatRow<Tags::Rot, Tags::SinAngle>> rotY;
    CachedRow<const FragmentGoalFoundTableTag> goalFound;
    for(const DBEvents::MoveCommand& cmd : events.toBeMovedElements) {
      const UnpackedDatabaseElementID& dest = ids.uncheckedUnpack(cmd.destination);
      //If this is one of the completed fragments enqueued to be moved to the completed table
      if(resolver.tryGetOrSwapRow(goalFound, dest)) {
        UnpackedDatabaseElementID self{ ids.uncheckedUnpack(cmd.source) };
        if(resolver.tryGetOrSwapAllRows(self, posX, posY, goalX, goalY, rotX, rotY)) {
          //Snap to destination
          //TODO: effects to celebrate transition
          const size_t si = self.getElementIndex();
          TableAdapters::write(si, TableAdapters::read(si, *goalX, *goalY), *posX, *posY);
          //This is no rotation, which will align with the image
          TableAdapters::write(si, glm::vec2{ 1, 0 }, *rotX, *rotY);
        }
      }
    }
  }

  void preProcessEvents(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("fragment events");
    using namespace Tags;
    auto resolver = task.getResolver<
      FloatRow<Pos, X>, FloatRow<Pos, Y>,
      FloatRow<Rot, X>, FloatRow<Rot, Y>,
      const FloatRow<FragmentGoal, X>, const FloatRow<FragmentGoal, Y>,
      const FragmentGoalFoundTableTag
    >();
    auto ids = task.getIDResolver();
    const DBEvents& events = Events::getPublishedEvents(task);
    task.setCallback([resolver, &events, ids](AppTaskArgs&) mutable {
      _prepareFragmentMigration(events, *resolver, *ids);
    });
    builder.submitTask(std::move(task));
  }

  //Check to see if each fragment has reached its goal
  void checkFragmentGoals(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("check fragment goals");
    using namespace Tags;
    auto query = task.query<
      const FloatRow<GPos, X>, const FloatRow<GPos, Y>,
      const FloatRow<FragmentGoal, X>, const FloatRow<FragmentGoal, Y>,
      FragmentGoalFoundRow
    >();
    const Config::GameConfig* config = TableAdapters::getGameConfig(task);

    task.setCallback([config, query](AppTaskArgs&) mutable {
      for(size_t t = 0; t < query.size(); ++t) {
        auto&& [posX, posY, goalX, goalY, goalFound] = query.get(t);

        ispc::UniformConstVec2 pos{ posX->data(), posY->data() };
        ispc::UniformConstVec2 goal{ goalX->data(), goalY->data() };
        const float minDistance = config->fragment.fragmentGoalDistance;

        ispc::checkFragmentGoals(pos, goal, goalFound->data(), minDistance, posX->size());
      }
    });

    builder.submitTask(std::move(task));
  }

  void updateFragmentGoals(IAppBuilder& builder) {
    checkFragmentGoals(builder);
    _migrateCompletedFragments(builder);
  }
}