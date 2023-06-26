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

    CameraAdapater camera = TableAdapters::getCamera(game);

    CameraTable& cameraTable = std::get<CameraTable>(db.mTables);
    TableOperations::stableResizeTable<GameDatabase>(cameraTable, 1, stableMappings);
    const size_t cameraIndex = 0;
    Camera& mainCamera = std::get<Row<Camera>>(cameraTable.mRows).at(0);
    mainCamera.zoom = 15.f;

    PlayerTable& players = std::get<PlayerTable>(db.mTables);
    TableOperations::stableResizeTable(players, UnpackedDatabaseElementID::fromPacked(GameDatabase::getTableIndex<PlayerTable>()), 1, stableMappings);
    std::get<FloatRow<Rot, CosAngle>>(players.mRows).at(0) = 1.0f;
    //Random angle in sort of radians
    const float playerStartAngle = float(generator() % 360)*6.282f/360.0f;
    const float playerStartDistance = 25.0f;
    //Start way off the screen, the world boundary will fling them into the scene
    std::get<FloatRow<Pos, X>>(players.mRows).at(0) = playerStartDistance*std::cos(playerStartAngle);
    std::get<FloatRow<Pos, Y>>(players.mRows).at(0) = playerStartDistance*std::sin(playerStartAngle);

    //Usually this would be done with a thread local but this setup is synchronous
    auto follow = TableAdapters::getCentralStatEffects(game).followTargetByPosition;
    const size_t id = TableAdapters::addStatEffectsSharedLifetime(follow.base, StatEffect::INFINITE, &camera.object.stable->at(cameraIndex), 1);
    follow.command->at(id).mode = FollowTargetByPositionStatEffect::FollowMode::Interpolation;
    follow.base.target->at(id) = StableElementID::fromStableID(TableAdapters::getPlayer(game).object.stable->at(0));
    follow.base.curveDefinition->at(id) = &Config::getCurve(TableAdapters::getConfig(game).game->camera.followCurve);

    //Add some arbitrary objects for testing
    const size_t rows = args.mFragmentRows;
    const size_t columns = args.mFragmentColumns;
    const size_t total = rows*columns;
    TableOperations::stableResizeTable(gameobjects, UnpackedDatabaseElementID::fromPacked(GameDatabase::getTableIndex<GameObjectTable>()), total, stableMappings);

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

    PhysicsSimulation::initialPopulateBroadphase({ db });
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
          //const size_t id = GameDatabase::getElementID
          GameObjectAdapter fragments = TableAdapters::getGameObjects(*args.db);
          const size_t id = fullId.getElementIndex();
          GameObjectAdapter destObjs = TableAdapters::getGameplayStaticGameObjects(*args.db);
          auto& destinationFragments = std::get<StaticGameObjectTable>(args.db->db.mTables);
          const size_t oldDestinationEnd = destObjs.stable->size();

          //Snap to destination
          //TODO: effects to celebrate transition
          GameObjectTable& fragmentsTable = std::get<GameObjectTable>(args.db->db.mTables);
          ispc::UniformConstVec2 goal = _unwrapConstFloatRow<FragmentGoal>(fragmentsTable);
          fragments.transform.posX->at(id) = goal.x[id];
          fragments.transform.posY->at(id) = goal.y[id];
          //This is no rotation, which will align with the image
          fragments.transform.rotX->at(id) = 1.0f;
          fragments.transform.rotY->at(id) = 0.0f;

          auto& oldMinX = std::get<SweepNPruneBroadphase::OldMinX>(fragmentsTable.mRows);
          auto& oldMinY = std::get<SweepNPruneBroadphase::OldMinY>(fragmentsTable.mRows);
          auto& keys = std::get<SweepNPruneBroadphase::Key>(fragmentsTable.mRows);
          auto& broadphase = std::get<BroadphaseTable>(args.db->db.mTables);
          SweepNPruneBroadphase::eraseRange(id, 1, broadphase, oldMinX, oldMinY, keys);

          constexpr GameDatabase::ElementID destinationTableID = GameDatabase::getTableIndex<StaticGameObjectTable>();
          StableElementMappings& mappings = TableAdapters::getStableMappings(*args.db);
          TableOperations::stableMigrateOne(fragmentsTable, destinationFragments, fullId, destinationTableID, mappings);

          //TODO: having to reinsert this here is awkward, it should be deferred to a particular part of the frame and ideally require less knowledge by the gameplay logic
          //Update the broadphase mappings in place here
          //If this becomes a more common occurence this table migration should be deferred so this update
          //can be done in a less fragile way
          auto& posX = *destObjs.transform.posX;
          auto& posY = *destObjs.transform.posY;
          //Scratch containers used to insert the elements, don't need to be stored since objects won't move after this
          Table<SweepNPruneBroadphase::NeedsReinsert,
            SweepNPruneBroadphase::OldMinX,
            SweepNPruneBroadphase::OldMinY,
            SweepNPruneBroadphase::OldMaxX,
            SweepNPruneBroadphase::OldMaxY,
            SweepNPruneBroadphase::NewMinX,
            SweepNPruneBroadphase::NewMinY,
            SweepNPruneBroadphase::NewMaxX,
            SweepNPruneBroadphase::NewMaxY> tempTable;
          TableOperations::resizeTable(tempTable, 1);

          auto config = PhysicsSimulation::_getStaticBoundariesConfig();

          SweepNPruneBroadphase::recomputeBoundaries(
            TableOperations::unwrapRow<SweepNPruneBroadphase::OldMinX>(tempTable),
            TableOperations::unwrapRow<SweepNPruneBroadphase::OldMaxX>(tempTable),
            TableOperations::unwrapRow<SweepNPruneBroadphase::NewMinX>(tempTable),
            TableOperations::unwrapRow<SweepNPruneBroadphase::NewMaxX>(tempTable),
            posX.mElements.data() + oldDestinationEnd,
            config,
            std::get<SweepNPruneBroadphase::NeedsReinsert>(tempTable.mRows));
          SweepNPruneBroadphase::recomputeBoundaries(
            TableOperations::unwrapRow<SweepNPruneBroadphase::OldMinY>(tempTable),
            TableOperations::unwrapRow<SweepNPruneBroadphase::OldMaxY>(tempTable),
            TableOperations::unwrapRow<SweepNPruneBroadphase::NewMinY>(tempTable),
            TableOperations::unwrapRow<SweepNPruneBroadphase::NewMaxY>(tempTable),
            posY.mElements.data() + oldDestinationEnd,
            config,
            std::get<SweepNPruneBroadphase::NeedsReinsert>(tempTable.mRows));

          Sweep2D& sweep = std::get<SharedRow<Sweep2D>>(broadphase.mRows).at();
          SweepNPruneBroadphase::PairChanges& changes = std::get<SharedRow<SweepNPruneBroadphase::PairChanges>>(broadphase.mRows).at();

          SweepNPrune::insertRange(
            sweep,
            TableOperations::_unwrapRowWithOffset<SweepNPruneBroadphase::NewMinX>(tempTable, 0),
            TableOperations::_unwrapRowWithOffset<SweepNPruneBroadphase::NewMinY>(tempTable, 0),
            TableOperations::_unwrapRowWithOffset<SweepNPruneBroadphase::NewMaxX>(tempTable, 0),
            TableOperations::_unwrapRowWithOffset<SweepNPruneBroadphase::NewMaxY>(tempTable, 0),
            //Point at the stable keys already in the destination, the rest comes from the temp table
            TableOperations::_unwrapRowWithOffset<StableIDRow>(destinationFragments, oldDestinationEnd),
            changes.mGained,
            1);
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