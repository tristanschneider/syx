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
#include "time/TimeModule.h"
#include "Events.h"
#include "TLSTaskImpl.h"
#include <transform/TransformRows.h>

namespace Fragment {
  using namespace Tags;

  FragmentTables::FragmentTables(RuntimeDatabaseTaskBuilder& task)
    : completeTable{ task.queryTables<FragmentGoalFoundTableTag>().tryGet() }
    , activeTable{ task.queryTables<FragmentSeekingGoalTagRow>().tryGet() }
  {}

  class FragmentMigrator : public IFragmentMigrator {
  public:
    FragmentMigrator(RuntimeDatabaseTaskBuilder& task)
      : ids{ task.getRefResolver() }
    {
      FragmentTables tables{ task };
      activeQuery = task.query<Events::EventsRow>(tables.activeTable);
      completeQuery = task.query<Events::EventsRow>(tables.completeTable);
      assert(activeQuery.size());
      assert(completeQuery.size());
    }

    void moveActiveToComplete(const ElementRef& activeFragment, AppTaskArgs&) final {
      if(auto e = ids.unpack(activeFragment); e && e.getTableIndex() == activeQuery.getTableID(0).getTableIndex()) {
        activeQuery.get<0>(0).getOrAdd(e.getElementIndex()).setMove(completeQuery.getTableID(0));
      }
    }

    void moveCompleteToActive(const ElementRef& completeFragment, AppTaskArgs&) final {
      if(auto e = ids.unpack(completeFragment); e && e.getTableIndex() == completeQuery.getTableID(0).getTableIndex()) {
        completeQuery.get<0>(0).getOrAdd(e.getElementIndex()).setMove(activeQuery.getTableID(0));
      }
    }

    QueryResult<Events::EventsRow> activeQuery, completeQuery;
    ElementRefResolver ids;
  };

  std::shared_ptr<IFragmentMigrator> createFragmentMigrator(RuntimeDatabaseTaskBuilder& task) {
    return std::make_shared<FragmentMigrator>(task);
  }

  void _migrateCompletedFragments(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("Migrate completed fragments");
    auto query = task.query<
      FragmentGoalFoundRow,
      const FragmentGoalCooldownRow,
      Events::EventsRow
    >();
    const TableID completedTable = builder.queryTables<FragmentGoalFoundTableTag>()[0];

    task.setCallback([query, completedTable](AppTaskArgs&) mutable {
      for(size_t t = 0; t < query.size(); ++t) {
        auto&& [goalFound, goalCooldown, events] = query.get(t);
        for(size_t i = 0; i < goalFound->size(); ++i) {
          //If the goal is found, enqueue a move request to the completed fragments table
          if(goalFound->at(i)) {
            //Goal was found but goal seeking is on cooldown, reset goal found flag and try again later
            if(goalCooldown->at(i)) {
              goalFound->at(i) = 0;
            }
            else {
              events->getOrAdd(i).setMove(completedTable);
            }
          }
        }
      }
    });

    builder.submitTask(std::move(task));
  }

  //Migration enqueues an event to request that table service moves the element to the given table
  //This event triggers just before the table service runs to prepare the state of the object for migration
  //It is not done when the event is queued because other forces could change the object before then and when the migration happens
  struct PrepareFragmentMigration {
    void init(RuntimeDatabaseTaskBuilder& task) {
      query = task;
      ids = task.getRefResolver();
      res = task.getResolver(fragmentFound);
    }

    void execute() {
      for(size_t t = 0; t < query.size(); ++t) {
        auto [events, transforms, goalX, goalY] = query.get(t);
        for(auto event : *events) {
          //If it's a move event into the fragment foal found table
          if(event.second.isMove() && res->tryGetOrSwapRow(fragmentFound, event.second.getTableID())) {
            const size_t si = event.first;
            //Snap to destination
            //TODO: effects to celebrate transition
            Transform::PackedTransform& transform = transforms->at(si);
            Transform::Parts parts = transform.decompose();
            parts.translate = { goalX->at(si), goalY->at(si), parts.translate.z };
            //This is no rotation, which will align with the image
            parts.rot = { 1, 0 };
            transform = Transform::PackedTransform::build(parts);
          }
        }
      }
    }

    QueryResult<
      const Events::EventsRow,
      Transform::WorldTransformRow,
      const Tags::FragmentGoalXRow, const Tags::FragmentGoalYRow
    > query;
    ElementRefResolver ids;
    CachedRow<const FragmentGoalFoundTableTag> fragmentFound;
    std::shared_ptr<ITableResolver> res;
  };

  struct CheckForFragmentReactivation {
    void init(RuntimeDatabaseTaskBuilder& task) {
      query = task;
    }

    void execute() {
      //Query all events in the active table that were moves. Presumably they moved from the completed table into the active table
      for(size_t t = 0; t < query.size(); ++t) {
        auto [events, goalCooldown] = query.get(t);
        for(auto event : *events) {
          if(event.second.isMove()) {
            //TODO: configurable
            goalCooldown->at(event.first) = 5;
          }
        }
      }
    }

    QueryResult<const Events::EventsRow, FragmentGoalCooldownRow> query;
  };

  void preProcessEvents(IAppBuilder& builder) {
    builder.submitTask(TLSTask::create<PrepareFragmentMigration>("fragment events"));
  }

  void postProcessEvents(IAppBuilder& builder) {
    builder.submitTask(TLSTask::create<CheckForFragmentReactivation>("fragment reactivation"));
  }

  //TODO: should use trigger volumes
  //Check to see if each fragment has reached its goal
  void checkFragmentGoals(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("check fragment goals");
    using namespace Tags;
    auto query = task.query<
      const Transform::WorldTransformRow,
      const FloatRow<FragmentGoal, X>, const FloatRow<FragmentGoal, Y>,
      FragmentGoalFoundRow
    >();
    const Config::GameConfig* config = TableAdapters::getGameConfig(task);

    task.setCallback([config, query](AppTaskArgs&) mutable {
      for(size_t t = 0; t < query.size(); ++t) {
        auto&& [transforms, goalX, goalY, goalFound] = query.get(t);
        const float minDistance = config->fragment.fragmentGoalDistance;

        for(size_t i = 0; i < transforms->size(); ++i) {
          const Transform::PackedTransform& pt = transforms->at(i);
          if(std::abs(pt.tx - goalX->at(i)) < minDistance && std::abs(pt.ty - goalY->at(i))) {
            goalFound->at(i) = 1;
          }
        }
      }
    });

    builder.submitTask(std::move(task));
  }

  void tickGoalCooldown(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("tick goal cooldown");
    auto query = task.query<
      FragmentGoalCooldownDefinitionRow,
      FragmentGoalCooldownRow
    >();
    const Time::TimeTransform* time = TimeModule::getSimTime(task);
    if(!time) {
      task.discard();
      return;
    }

    task.setCallback([query, time](AppTaskArgs&) mutable {
      for(size_t t = 0; t < query.size(); ++t) {
        auto&& [def, cooldowns] = query.get(t);
        FragmentGoalCooldownDefinition& d = def->at();
        d.currentTime += time->getSecondsToTicks();
        if(d.currentTime < d.timeToTick) {
          continue;
        }

        for(FragmentCooldownT& cd : *cooldowns) {
          if(cd > 0) {
            --cd;
          }
        }
      }
    });

    builder.submitTask(std::move(task));
  }

  void updateFragmentGoals(IAppBuilder& builder) {
    tickGoalCooldown(builder);
    checkFragmentGoals(builder);
    _migrateCompletedFragments(builder);
  }
}