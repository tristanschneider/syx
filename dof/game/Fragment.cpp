#include "Precompile.h"
#include "Fragment.h"

#include <random>
#include "PhysicsSimulation.h"
#include "Simulation.h"
#include "TableAdapters.h"
#include "unity.h"

#include "glm/gtx/norm.hpp"
#include "stat/AllStatEffects.h"

namespace Fragment {
  using namespace Tags;

  template<class Tag, class TableT>
  ispc::UniformConstVec2 _unwrapConstFloatRow(TableT& t) {
    return { std::get<FloatRow<Tag, X>>(t.mRows).mElements.data(), std::get<FloatRow<Tag, Y>>(t.mRows).mElements.data() };
  }

  void _setupScene(GameDB game, SceneArgs args) {
    GameDatabase& db = game.db;
    std::random_device device;
    std::mt19937 generator(device());

    auto& stableMappings = TableAdapters::getStableMappings({ db });
    GameObjectTable& gameobjects = std::get<GameObjectTable>(db.mTables);
    SceneState& scene = std::get<0>(std::get<GlobalGameData>(db.mTables).mRows).at();

    //Add some arbitrary objects for testing
    const size_t rows = args.mFragmentRows;
    const size_t columns = args.mFragmentColumns;
    const size_t total = rows*columns;
    TableOperations::stableResizeTable(gameobjects, UnpackedDatabaseElementID::fromPacked(GameDatabase::getTableIndex<GameObjectTable>()), total, stableMappings);
    StableIDRow& stableRow = std::get<StableIDRow>(gameobjects.mRows);
    for(size_t i = 0; i < total; ++i) {
      Events::onNewElement(StableElementID::fromStableRow(i, stableRow), game);
    }

    float startX = -float(columns)/2.0f;
    float startY = -float(rows)/2.0f;
    float scaleX = 1.0f/float(columns);
    float scaleY = 1.0f/float(rows);
    auto& posX = std::get<FloatRow<Pos, X>>(gameobjects.mRows);
    auto& posY = std::get<FloatRow<Pos, Y>>(gameobjects.mRows);
    auto& goalX = std::get<FloatRow<FragmentGoal, X>>(gameobjects.mRows);
    auto& goalY = std::get<FloatRow<FragmentGoal, Y>>(gameobjects.mRows);

    //Shuffle indices randomly
    std::vector<size_t> indices(rows*columns);
    int counter = 0;
    std::generate(indices.begin(), indices.end(), [&counter] { return counter++; });
    std::shuffle(indices.begin(), indices.end(), generator);

    for(size_t j = 0; j < total; ++j) {
      const size_t shuffleIndex = indices[j];
      CubeSprite& sprite = std::get<Row<CubeSprite>>(gameobjects.mRows).at(j);
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

      std::get<FloatRow<Rot, CosAngle>>(gameobjects.mRows).at(j) = 1.0f;
    }

    const float boundaryPadding = 10.0f;
    const size_t first = 0;
    const size_t last = total - 1;
    scene.mBoundaryMin = glm::vec2(goalX.at(first), goalY.at(first)) - glm::vec2(boundaryPadding);
    scene.mBoundaryMax = glm::vec2(goalX.at(last), goalY.at(last)) + glm::vec2(boundaryPadding);
  }

  void _migrateCompletedFragments(GameObjectTable& fragments, Events::MovePublisher moveElement) {
    PROFILE_SCOPE("simulation", "migratefragments");
    //If the goal was found, move them to the destination table.
    //Do this in reverse so the swap remove doesn't mess up a previous removal
    const size_t oldTableSize = TableOperations::size(fragments);
    const uint8_t* goalFound = TableOperations::unwrapRow<FragmentGoalFoundRow>(fragments);
    const auto& stableRow = std::get<StableIDRow>(fragments.mRows);

    constexpr auto dstTable = GameDatabase::getTableIndex<StaticGameObjectTable>();
    for(size_t i = 0; i < oldTableSize; ++i) {
      //If the goal is found, enqueue a move request to the completed fragments table
      if(goalFound[i]) {
        moveElement(StableElementID::fromStableRow(i, stableRow), UnpackedDatabaseElementID::fromPacked(dstTable));
      }
    }
  }

  using MigrationResolver = TableResolver<
    FloatRow<Tags::Pos, Tags::X>,
    FloatRow<Tags::Pos, Tags::Y>,
    FloatRow<Tags::FragmentGoal, Tags::X>,
    FloatRow<Tags::FragmentGoal, Tags::Y>,
    FloatRow<Tags::Rot, Tags::CosAngle>,
    FloatRow<Tags::Rot, Tags::SinAngle>
  >;
  //Migration enqueues an event to request that table service moves the element to the given table
  //This event triggers just before the table service runs to prepare the state of the object for migration
  //It is not done when the event is queued because other forces could change the object before then and when the migration happens
  void _prepareFragmentMigration(const DBEvents& events, MigrationResolver& resolver) {
    CachedRow<FloatRow<Tags::Pos, Tags::X>> posX;
    CachedRow<FloatRow<Tags::Pos, Tags::Y>> posY;
    CachedRow<FloatRow<Tags::FragmentGoal, Tags::X>> goalX;
    CachedRow<FloatRow<Tags::FragmentGoal, Tags::Y>> goalY;
    CachedRow<FloatRow<Tags::Rot, Tags::CosAngle>> rotX;
    CachedRow<FloatRow<Tags::Rot, Tags::SinAngle>> rotY;
    for(const DBEvents::MoveCommand& cmd : events.toBeMovedElements) {
      //If this is one of the completed fragments enqueued to be moved to the completed table
      if(cmd.destination.toPacked<GameDatabase>().getTableIndex() == GameDatabase::getTableIndex<StaticGameObjectTable>().getTableIndex()) {
        UnpackedDatabaseElementID self { UnpackedDatabaseElementID::fromPacked(cmd.source.toPacked<GameDatabase>()) };
        resolver.tryGetOrSwapRow(posX, self);
        resolver.tryGetOrSwapRow(posY, self);
        resolver.tryGetOrSwapRow(goalX, self);
        resolver.tryGetOrSwapRow(goalY, self);
        resolver.tryGetOrSwapRow(rotX, self);
        resolver.tryGetOrSwapRow(rotY, self);
        if(posX && goalX) {
          //Snap to destination
          //TODO: effects to celebrate transition
          const size_t si = self.getElementIndex();
          posX->at(si) = goalX->at(si);
          posY->at(si) = goalY->at(si);
          //This is no rotation, which will align with the image
          TableAdapters::write(si, glm::vec2{ 1, 0 }, *rotX, *rotY);
        }
      }
    }
  }

  TaskRange processEvents(GameDB db) {
    auto root = TaskNode::create([db](...) {
      auto resolver = MigrationResolver::create(db.db);
      const DBEvents& events = Events::getPublishedEvents(db);
      _prepareFragmentMigration(events, resolver);
    });
    return TaskBuilder::addEndSync(root);
  }

  //Read GPos, FragmentGoal
  //Write FragmentGoalFoundRow
  //Check to see if each fragment has reached its goal
  void _checkFragmentGoals(GameObjectTable& fragments, const Config::GameConfig& config) {
    PROFILE_SCOPE("simulation", "fragmentgoals");
    ispc::UniformConstVec2 pos = _unwrapConstFloatRow<GPos>(fragments);
    ispc::UniformConstVec2 goal = _unwrapConstFloatRow<FragmentGoal>(fragments);
    uint8_t* goalFound = TableOperations::unwrapRow<FragmentGoalFoundRow>(fragments);
    const float minDistance = config.fragment.fragmentGoalDistance;

    ispc::checkFragmentGoals(pos, goal, goalFound, minDistance, TableOperations::size(fragments));
  }

  void setupScene(GameDB db) {
    SceneArgs args;
    const Config::GameConfig* config = TableAdapters::getConfig({ db }).game;
    args.mFragmentRows = config->fragment.fragmentRows;
    args.mFragmentColumns = config->fragment.fragmentColumns;
    _setupScene(db, args);
  }

  //Read FragmentGoalFoundRow, StableIDRow
  //Modify thread locals
  void _migrateCompletedFragments(GameDB game, size_t) {
    auto& gameObjects = std::get<GameObjectTable>(game.db.mTables);
    _migrateCompletedFragments(gameObjects, Events::createMovePublisher(game));
  }

  TaskRange updateFragmentGoals(GameDB game) {
    auto task = TaskNode::create([game](enki::TaskSetPartition, uint32_t thread) {
      GameDatabase& db = game.db;
      auto& gameObjects = std::get<GameObjectTable>(db.mTables);
      auto config = TableAdapters::getConfig(game).game;

      _checkFragmentGoals(gameObjects, *config);
      _migrateCompletedFragments({ db }, thread);
    });
    return TaskBuilder::addEndSync(task);
  }
}