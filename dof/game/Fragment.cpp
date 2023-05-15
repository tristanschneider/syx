#include "Precompile.h"
#include "Fragment.h"

#include <random>
#include "PhysicsSimulation.h"
#include "Simulation.h"
#include "TableAdapters.h"
#include "unity.h"

#include "glm/gtx/norm.hpp"

namespace Fragment {
  using namespace Tags;

  //TODO: generalize this in a way that it can be used like TableAdapters. Probably requires runtime abstraction for Table types
  struct PositionReader {
    template<class TableT>
    PositionReader(TableT& t)
      : x(std::get<FloatRow<Tags::Pos, Tags::X>>(t.mRows))
      , y(std::get<FloatRow<Tags::Pos, Tags::Y>>(t.mRows)) {
    }

    glm::vec2 at(size_t i) const {
      return { x.at(i), y.at(i) };
    }

    FloatRow<Tags::Pos, Tags::X>& x;
    FloatRow<Tags::Pos, Tags::Y>& y;
  };

  struct RotationReader {
    template<class TableT>
    RotationReader(TableT& t)
      : sinAngle(std::get<FloatRow<Tags::Rot, Tags::SinAngle>>(t.mRows))
      , cosAngle(std::get<FloatRow<Tags::Rot, Tags::CosAngle>>(t.mRows)) {
    }

    FloatRow<Tags::Rot, Tags::CosAngle>& cosAngle;
    FloatRow<Tags::Rot, Tags::SinAngle>& sinAngle;
  };

  struct VelocityReader {
    template<class TableT>
    VelocityReader(TableT& t)
      : linVelX(std::get<FloatRow<Tags::LinVel, Tags::X>>(t.mRows))
      , linVelY(std::get<FloatRow<Tags::LinVel, Tags::Y>>(t.mRows))
      , angVel(std::get<FloatRow<Tags::AngVel, Tags::Angle>>(t.mRows)) {
    }

    glm::vec2 getLinVel(size_t i) const {
      return { linVelX.at(i), linVelY.at(i) };
    }

    float getAngVel(size_t i) const {
      return angVel.at(i);
    }

    void setAngVel(size_t i, float value) {
      angVel.at(i) = value;
    }

    void setLinVel(size_t i, const glm::vec2& value) {
      linVelX.at(i) = value.x;
      linVelY.at(i) = value.y;
    }

    FloatRow<Tags::LinVel, Tags::X>& linVelX;
    FloatRow<Tags::LinVel, Tags::Y>& linVelY;
    FloatRow<Tags::AngVel, Tags::Angle>& angVel;
  };

  struct FragmentForceEdge {
    glm::vec2 point{};
    float contribution{};
  };

  FragmentForceEdge _getEdge(const glm::vec2& normalizedForceDir, const glm::vec2& fragmentNormal, const glm::vec2& fragmentPos, float size) {
    const float cosAngle = glm::dot(normalizedForceDir, fragmentNormal);
    //Constribution is how close to aligned the force and normal are, point is position then normal in direction of force
    if(cosAngle >= 0.0f) {
      return { fragmentPos - fragmentNormal*size, 1.0f - cosAngle };
    }
    return { fragmentPos + fragmentNormal*size, 1.0f + cosAngle };
  }

  float crossProduct(const glm::vec2& a, const glm::vec2& b) {
    //[ax] x [bx] = [ax*by - ay*bx]
    //[ay]   [by]
    return a.x*b.y - a.y*b.x;
  }

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

    CameraTable& camera = std::get<CameraTable>(db.mTables);
    TableOperations::addToTable(camera);
    Camera& mainCamera = std::get<Row<Camera>>(camera.mRows).at(0);
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

  void _migrateCompletedFragments(GameObjectTable& fragments, StaticGameObjectTable& destinationFragments, BroadphaseTable& broadphase, StableElementMappings& mappings) {
    PROFILE_SCOPE("simulation", "migratefragments");
    //If the goal was found, move them to the destination table.
    //Do this in reverse so the swap remove doesn't mess up a previous removal
    const size_t oldTableSize = TableOperations::size(fragments);
    const size_t oldDestinationEnd = TableOperations::size(destinationFragments);
    uint8_t* goalFound = TableOperations::unwrapRow<FragmentGoalFoundRow>(fragments);
    ispc::UniformConstVec2 goal = _unwrapConstFloatRow<FragmentGoal>(fragments);

    for(size_t i = 0; i < oldTableSize; ++i) {
      const size_t reverseIndex = oldTableSize - i - 1;
      if(goalFound[reverseIndex]) {
        //Snap to destination
        //TODO: effects to celebrate transition
        std::get<FloatRow<Pos, X>>(fragments.mRows).at(reverseIndex) = goal.x[reverseIndex];
        std::get<FloatRow<Pos, Y>>(fragments.mRows).at(reverseIndex) = goal.y[reverseIndex];
        //This is no rotation, which will align with the image
        std::get<FloatRow<Rot, CosAngle>>(fragments.mRows).at(reverseIndex) = 1.0f;
        std::get<FloatRow<Rot, SinAngle>>(fragments.mRows).at(reverseIndex) = 0.0f;

        auto& oldMinX = std::get<SweepNPruneBroadphase::OldMinX>(fragments.mRows);
        auto& oldMinY = std::get<SweepNPruneBroadphase::OldMinY>(fragments.mRows);
        auto& keys = std::get<SweepNPruneBroadphase::Key>(fragments.mRows);
        SweepNPruneBroadphase::eraseRange(reverseIndex, 1, broadphase, oldMinX, oldMinY, keys);

        GameDatabase::ElementID removeIndex{ GameDatabase::getTableIndex<GameObjectTable>().getTableIndex(), reverseIndex };
        constexpr GameDatabase::ElementID destinationTableID = GameDatabase::getTableIndex<StaticGameObjectTable>();
        TableOperations::stableMigrateOne(fragments, destinationFragments, removeIndex, destinationTableID, mappings);
      }
    }

    //TODO: having to reinsert this here is awkward, it should be deferred to a particular part of the frame and ideally require less knowledge by the gameplay logic
    //Update the broadphase mappings in place here
    //If this becomes a more common occurence this table migration should be deferred so this update
    //can be done in a less fragile way
    const size_t newElements = TableOperations::size(destinationFragments) - oldDestinationEnd;
    if(newElements > 0) {
      auto& posX = std::get<FloatRow<Pos, X>>(destinationFragments.mRows);
      auto& posY = std::get<FloatRow<Pos, Y>>(destinationFragments.mRows);
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
      TableOperations::resizeTable(tempTable, newElements);

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
        newElements);
    }
  }

  //Check to see if each fragment has reached its goal
  void _checkFragmentGoals(GameObjectTable& fragments, const GameConfig& config) {
    PROFILE_SCOPE("simulation", "fragmentgoals");
    ispc::UniformConstVec2 pos = _unwrapConstFloatRow<Pos>(fragments);
    ispc::UniformConstVec2 goal = _unwrapConstFloatRow<FragmentGoal>(fragments);
    uint8_t* goalFound = TableOperations::unwrapRow<FragmentGoalFoundRow>(fragments);
    const float minDistance = config.fragmentGoalDistance;

    ispc::checkFragmentGoals(pos, goal, goalFound, minDistance, TableOperations::size(fragments));
  }

  void setupScene(GameDB db) {
    SceneArgs args;
    const GameConfig* config = TableAdapters::getConfig({ db }).game;
    args.mFragmentRows = config->fragmentRows;
    args.mFragmentColumns = config->fragmentColumns;
    _setupScene(db, args);
  }

  void _migrateCompletedFragments(GameDB game) {
    GameDatabase& db = game.db;
    auto& gameObjects = std::get<GameObjectTable>(db.mTables);
    auto& staticGameObjects = std::get<StaticGameObjectTable>(db.mTables);
    auto& broadphase = std::get<BroadphaseTable>(db.mTables);
    auto& stableMappings = TableAdapters::getStableMappings(game);
    _migrateCompletedFragments(gameObjects, staticGameObjects, broadphase, stableMappings);
  }

  TaskRange updateFragmentGoals(GameDB game) {
    auto task = TaskNode::create([game](...) {
      GameDatabase& db = game.db;
      auto& gameObjects = std::get<GameObjectTable>(db.mTables);
      auto& staticGameObjects = std::get<StaticGameObjectTable>(db.mTables);
      auto& broadphase = std::get<BroadphaseTable>(db.mTables);
      auto& stableMappings = TableAdapters::getStableMappings(game);
      auto config = TableAdapters::getConfig(game).game;

      _checkFragmentGoals(gameObjects, *config);
      _migrateCompletedFragments(gameObjects, staticGameObjects, broadphase, stableMappings);
    });
    return TaskBuilder::addEndSync(task);
  }

  void _tickForceLifetimes(GlobalPointForceTable& table) {
    PROFILE_SCOPE("simulation", "forcelifetime");
    auto& lifetime = std::get<ForceData::Lifetime>(table.mRows);
    for(size_t i = 0; i < lifetime.size();) {
      size_t& current = lifetime.at(i);
      if(!current) {
        TableOperations::swapRemove(table, i);
      }
      else {
        --current;
        ++i;
      }
    }
  }

  void _applyForces(GlobalPointForceTable& forces, GameObjectTable& fragments) {
    PROFILE_SCOPE("simulation", "forces");
    PositionReader forcePosition(forces);
    PositionReader fragmentPosition(fragments);
    RotationReader fragmentRotation(fragments);
    VelocityReader fragmentVelocity(fragments);

    auto& strength = std::get<ForceData::Strength>(forces.mRows);
    for(size_t i = 0; i < TableOperations::size(fragments); ++i) {
      glm::vec2 right{ fragmentRotation.cosAngle.at(i), fragmentRotation.sinAngle.at(i) };
      glm::vec2 up{ right.y, -right.x };
      glm::vec2 fragmentPos = fragmentPosition.at(i);
      glm::vec2 fragmentLinVel = fragmentVelocity.getLinVel(i);
      float fragmentAngVel = fragmentVelocity.getAngVel(i);
      for(size_t f = 0; f < TableOperations::size(forces); ++f) {
        glm::vec2 forcePos = forcePosition.at(f);
        glm::vec2 impulse = fragmentPos - forcePos;
        float distance = glm::length(impulse);
        if(distance < 0.0001f) {
          impulse = glm::vec2(1.0f, 0.0f);
          distance = 1.0f;
        }
        //Linear falloff for now
        //TODO: something like easing for more interesting forces
        const float scalar = strength.at(f)/distance;
        //Normalize and scale to strength
        const glm::vec2 impulseDir = impulse/distance;
        impulse *= scalar;

        //Determine point to apply force at. Realistically this would be something like the center of pressure
        //Computing that is confusing so I'll hack at it instead
        //Take the two leading edges facing the force direction, then weight them based on their angle against the force
        //If the edge is head on that means only the one edge would matter
        //If the two edges were exactly 45 degrees from the force direction then the center of the two edges is chosen
        const float size = 0.5f;
        FragmentForceEdge edgeA = _getEdge(impulseDir, right, fragmentPos, size);
        FragmentForceEdge edgeB = _getEdge(impulseDir, right, fragmentPos, size);
        const glm::vec2 impulsePoint = edgeA.point*edgeA.contribution + edgeB.point*edgeB.contribution;

        fragmentLinVel += impulse;
        glm::vec2 r = impulsePoint - fragmentPos;
        fragmentAngVel += crossProduct(r, impulse);
      }

      fragmentVelocity.setLinVel(i, fragmentLinVel);
      fragmentVelocity.setAngVel(i, fragmentAngVel);
    }
  }

  void updateFragmentForces(GameDB game) {
    auto& forces = std::get<GlobalPointForceTable>(game.db.mTables);
    auto& gameObjects = std::get<GameObjectTable>(game.db.mTables);
    _applyForces(forces, gameObjects);
    _tickForceLifetimes(forces);
  }
}