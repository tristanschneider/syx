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

    const float boundaryPadding = 1.0f;
    const size_t first = 0;
    const size_t last = total - 1;
    scene.mBoundaryMin = glm::vec2(goalX.at(first), goalY.at(first)) - glm::vec2(boundaryPadding);
    scene.mBoundaryMax = glm::vec2(goalX.at(last), goalY.at(last)) + glm::vec2(boundaryPadding);
  }

  void _migrateCompletedFragments(GameObjectTable& fragments, LambdaStatEffectAdapter lambdaEffect) {
    PROFILE_SCOPE("simulation", "migratefragments");
    //If the goal was found, move them to the destination table.
    //Do this in reverse so the swap remove doesn't mess up a previous removal
    const size_t oldTableSize = TableOperations::size(fragments);
    const uint8_t* goalFound = TableOperations::unwrapRow<FragmentGoalFoundRow>(fragments);
    const auto& stableRow = std::get<StableIDRow>(fragments.mRows);

    for(size_t i = 0; i < oldTableSize; ++i) {
      const size_t reverseIndex = oldTableSize - i - 1;
      if(goalFound[reverseIndex]) {
        const size_t lid = lambdaEffect.command->size();
        auto& mod = lambdaEffect.base.modifier;
        mod.modifier.resize(mod.table, lid + 1, *mod.stableMappings);
        lambdaEffect.base.owner->at(lid) = StableElementID::fromStableRow(reverseIndex, stableRow);
        lambdaEffect.base.lifetime->at(lid) = StatEffect::INSTANT;
        lambdaEffect.command->at(lid) = [](LambdaStatEffect::Args& args) {
          const GameDatabase::ElementID fullId{ args.resolvedID.mUnstableIndex };
          //Make sure it's still in the table to migrate from
          if(fullId.getTableIndex() != GameDatabase::getTableIndex<GameObjectTable>().getTableIndex()) {
            return;
          }
          GameObjectAdapter fragments = TableAdapters::getGameObjects(*args.db);
          const size_t id = fullId.getElementIndex();
          GameObjectAdapter destObjs = TableAdapters::getGameplayStaticGameObjects(*args.db);
          auto& destinationFragments = std::get<StaticGameObjectTable>(args.db->db.mTables);

          //Snap to destination
          //TODO: effects to celebrate transition
          GameObjectTable& fragmentsTable = std::get<GameObjectTable>(args.db->db.mTables);
          ispc::UniformConstVec2 goal = _unwrapConstFloatRow<FragmentGoal>(fragmentsTable);
          fragments.transform.posX->at(id) = goal.x[id];
          fragments.transform.posY->at(id) = goal.y[id];
          //This is no rotation, which will align with the image
          fragments.transform.rotX->at(id) = 1.0f;
          fragments.transform.rotY->at(id) = 0.0f;

          constexpr GameDatabase::ElementID destinationTableID = GameDatabase::getTableIndex<StaticGameObjectTable>();
          StableElementMappings& mappings = TableAdapters::getStableMappings(*args.db);
          TableOperations::stableMigrateOne(fragmentsTable, destinationFragments, fullId, destinationTableID, mappings);
          Events::onMovedElement(args.resolvedID, *args.db);
        };
      }
    }
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
  void _migrateCompletedFragments(GameDB game, size_t thread) {
    auto& gameObjects = std::get<GameObjectTable>(game.db.mTables);
    _migrateCompletedFragments(gameObjects, TableAdapters::getLambdaEffects(game, thread));
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