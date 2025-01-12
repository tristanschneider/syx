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
#include "GameTime.h"

namespace Fragment {
  using namespace Tags;

  FragmentTables::FragmentTables(RuntimeDatabaseTaskBuilder& task)
    : completeTable{ task.queryTables<FragmentGoalFoundTableTag>().tryGet() }
    , activeTable{ task.queryTables<FragmentSeekingGoalTagRow>().tryGet() }
  {}

  class FragmentMigrator : public IFragmentMigrator {
  public:
    FragmentMigrator(RuntimeDatabaseTaskBuilder& task)
      : tables{ task }
    {}

    void moveActiveToComplete(const ElementRef& activeFragment, AppTaskArgs& args) final {
      Events::MovePublisher{ &args }(activeFragment, tables.completeTable);
    }

    void moveCompleteToActive(const ElementRef& completeFragment, AppTaskArgs& args) final {
      Events::MovePublisher{ &args}(completeFragment, tables.activeTable);
    }

    FragmentTables tables;
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
      const StableIDRow
    >();
    const TableID completedTable = builder.queryTables<FragmentGoalFoundTableTag>()[0];

    task.setCallback([query, completedTable](AppTaskArgs& args) mutable {
      Events::MovePublisher moveElement{{ &args }};
      for(size_t t = 0; t < query.size(); ++t) {
        auto&& [goalFound, goalCooldown, stableRow] = query.get(t);
        for(size_t i = 0; i < goalFound->size(); ++i) {
          //If the goal is found, enqueue a move request to the completed fragments table
          if(goalFound->at(i)) {
            //Goal was found but goal seeking is on cooldown, reset goal found flag and try again later
            if(goalCooldown->at(i)) {
              goalFound->at(i) = 0;
            }
            else {
              moveElement(stableRow->at(i), completedTable);
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
  void _prepareFragmentMigration(const DBEvents& events, ITableResolver& resolver, IIDResolver& ids) {
    CachedRow<FloatRow<Tags::Pos, Tags::X>> posX;
    CachedRow<FloatRow<Tags::Pos, Tags::Y>> posY;
    CachedRow<const FloatRow<Tags::FragmentGoal, Tags::X>> goalX;
    CachedRow<const FloatRow<Tags::FragmentGoal, Tags::Y>> goalY;
    CachedRow<FloatRow<Tags::Rot, Tags::CosAngle>> rotX;
    CachedRow<FloatRow<Tags::Rot, Tags::SinAngle>> rotY;
    CachedRow<const FragmentGoalFoundTableTag> goalFound;
    for(const DBEvents::MoveCommand& cmd : events.toBeMovedElements) {
      const TableID* destination = std::get_if<TableID>(&cmd.destination);
      const ElementRef* source = std::get_if<ElementRef>(&cmd.source);
      //const UnpackedDatabaseElementID& dest = ids.uncheckedUnpack(cmd.destination);
      //If this is one of the completed fragments enqueued to be moved to the completed table
      if(source && destination && resolver.tryGetOrSwapRow(goalFound, *destination)) {
        const std::optional<UnpackedDatabaseElementID> self = ids.getRefResolver().tryUnpack(*source);
        if(self && resolver.tryGetOrSwapAllRows(*self, posX, posY, goalX, goalY, rotX, rotY)) {
          //Snap to destination
          //TODO: effects to celebrate transition
          const size_t si = self->getElementIndex();
          TableAdapters::write(si, TableAdapters::read(si, *goalX, *goalY), *posX, *posY);
          //This is no rotation, which will align with the image
          TableAdapters::write(si, glm::vec2{ 1, 0 }, *rotX, *rotY);
        }
      }
    }
  }

  void checkForFragmentReactivation(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("check for reactivation");
    FragmentTables tables{ task };
    auto ids = task.getIDResolver()->getRefResolver();
    auto resolver = task.getResolver<FragmentGoalCooldownRow>();
    const DBEvents& events = Events::getPublishedEvents(task);

    task.setCallback([ids, resolver, &events, tables](AppTaskArgs&) {
      CachedRow<FragmentGoalCooldownRow> cooldown;
      for(const auto& move : events.toBeMovedElements) {
        //If this moved to the active table, presumably it came from the completed table
        if(auto destination = std::get_if<TableID>(&move.destination); destination && *destination == tables.activeTable) {
          if(auto moved = std::get_if<ElementRef>(&move.source); moved && *moved) {
            if(FragmentCooldownT* cd = resolver->tryGetOrSwapRowElement(cooldown, ids.uncheckedUnpack(*moved))) {
              //TODO: configurable
              *cd = 5;
            }
          }
        }
      }
    });

    builder.submitTask(std::move(task));
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

  void postProcessEvents(IAppBuilder& builder) {
    checkForFragmentReactivation(builder);
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

  void tickGoalCooldown(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("tick goal cooldown");
    auto query = task.query<
      FragmentGoalCooldownDefinitionRow,
      FragmentGoalCooldownRow
    >();
    const float* dt = GameTime::getDeltaTime(task);
    if(!dt) {
      task.discard();
      return;
    }

    task.setCallback([query, dt](AppTaskArgs&) mutable {
      for(size_t t = 0; t < query.size(); ++t) {
        auto&& [def, cooldowns] = query.get(t);
        FragmentGoalCooldownDefinition& d = def->at();
        d.currentTime += *dt;
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