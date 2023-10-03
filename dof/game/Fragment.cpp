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

namespace Fragment {
  using namespace Tags;

  void _setupScene(IAppBuilder& builder, SceneArgs args) {
    auto task = builder.createTask();
    task.setName("Fragment Setup");
    SceneState* scene = task.query<SharedRow<SceneState>>().tryGetSingletonElement();
    auto fragments = task.query<
      FloatRow<Pos, X>,
      FloatRow<Pos, Y>,
      FloatRow<FragmentGoal, X>,
      FloatRow<FragmentGoal, Y>,
      Row<CubeSprite>,
      const StableIDRow
    >();
    auto modifiers = task.getModifiersForTables(fragments.matchingTableIDs);

    task.setCallback([scene, fragments, modifiers, args](AppTaskArgs& taskArgs) mutable {
      if(scene->mState != SceneState::State::SetupScene) {
        return;
      }

      std::random_device device;
      std::mt19937 generator(device());

      //Add some arbitrary objects for testing
      const size_t rows = args.mFragmentRows;
      const size_t columns = args.mFragmentColumns;
      const size_t total = rows*columns;
      const float startX = -float(columns)/2.0f;
      const float startY = -float(rows)/2.0f;
      const float scaleX = 1.0f/float(columns);
      const float scaleY = 1.0f/float(rows);
      for(size_t t = 0; t < fragments.size(); ++t) {
        modifiers[t]->resize(total);
        auto tableRows = fragments.get(t);
        const StableIDRow& stableIDs = fragments.get<const StableIDRow>(t);
        for(size_t s = 0; s < stableIDs.size(); ++s) {
          Events::onNewElement(StableElementID::fromStableRow(s, stableIDs), taskArgs);
        }

        auto& posX = *std::get<FloatRow<Pos, X>*>(tableRows);
        auto& posY = *std::get<FloatRow<Pos, Y>*>(tableRows);
        auto& goalX = *std::get<FloatRow<FragmentGoal, X>*>(tableRows);
        auto& goalY = *std::get<FloatRow<FragmentGoal, Y>*>(tableRows);
        //Shuffle indices randomly
        std::vector<size_t> indices(rows*columns);
        int counter = 0;
        std::generate(indices.begin(), indices.end(), [&counter] { return counter++; });
        std::shuffle(indices.begin(), indices.end(), generator);

        for(size_t j = 0; j < total; ++j) {
          const size_t shuffleIndex = indices[j];
          CubeSprite& sprite = std::get<Row<CubeSprite>*>(tableRows)->at(j);
          const size_t row = j / columns;
          const size_t column = j % columns;
          const size_t shuffleRow = shuffleIndex / columns;
          const size_t shuffleColumn = shuffleIndex % columns;
          //Goal position and uv is based on original index, starting position is based on shuffled index
          sprite.uMin = float(column)/float(columns);
          sprite.vMin = float(row)/float(rows);
          sprite.uMax = sprite.uMin + scaleX;
          sprite.vMax = sprite.vMin + scaleY;

          goalX.at(j) = startX + sprite.uMin*float(columns);
          goalY.at(j) = startY + sprite.vMin*float(rows);

          posX.at(j) = startX + shuffleColumn;
          posY.at(j) = startY + shuffleRow;
        }

        const float boundaryPadding = 10.0f;
        const size_t first = 0;
        const size_t last = total - 1;
        scene->mBoundaryMin = glm::vec2(goalX.at(first), goalY.at(first)) - glm::vec2(boundaryPadding);
        scene->mBoundaryMax = glm::vec2(goalX.at(last), goalY.at(last)) + glm::vec2(boundaryPadding);
      }
    });
    builder.submitTask(std::move(task));
  }

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
    CachedRow<const FragmentGoalFoundRow> goalFound;
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

  void setupScene(IAppBuilder& builder) {
    auto temp = builder.createTask();
    const Config::GameConfig* config = temp.query<const SharedRow<Config::GameConfig>>().tryGetSingletonElement();
    temp.discard();

    SceneArgs args;
    args.mFragmentRows = config->fragment.fragmentRows;
    args.mFragmentColumns = config->fragment.fragmentColumns;
    _setupScene(builder, args);
  }

  void updateFragmentGoals(IAppBuilder& builder) {
    checkFragmentGoals(builder);
    _migrateCompletedFragments(builder);
  }
}