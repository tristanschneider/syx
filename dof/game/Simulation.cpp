#include "Precompile.h"
#include "Simulation.h"

#include "Queries.h"

#include "unity.h"

#include "glm/gtx/norm.hpp"
#include <random>
#include "Serializer.h"
#include "Profile.h"

namespace {
  using namespace Tags;

  template<class RowT, class TableT>
  decltype(std::declval<RowT&>().mElements.data()) _unwrapRow(TableT& t) {
    if constexpr(TableOperations::hasRow<RowT, TableT>()) {
      return std::get<RowT>(t.mRows).mElements.data();
    }
    else {
      return nullptr;
    }
  }

  template<class Tag, class TableT>
  ispc::UniformConstVec2 _unwrapConstFloatRow(TableT& t) {
    return { std::get<FloatRow<Tag, X>>(t.mRows).mElements.data(), std::get<FloatRow<Tag, Y>>(t.mRows).mElements.data() };
  }

  PhysicsConfig _getPhysicsConfig() {
    PhysicsConfig result;
    return result;
  }

  SweepNPruneBroadphase::BoundariesConfig _getBoundariesConfig() {
    SweepNPruneBroadphase::BoundariesConfig result;
    return result;
  }

  SweepNPruneBroadphase::BoundariesConfig _getStaticBoundariesConfig() {
    //Can fit more snugly since they are axis aligned
    SweepNPruneBroadphase::BoundariesConfig result;
    result.mPadding = 0.0f;
    return result;
  }

  StableElementMappings& _getStableMappings(GameDatabase& db) {
    return std::get<SharedRow<StableElementMappings>>(std::get<GlobalGameData>(db.mTables).mRows).at();
  }

  size_t _requestTextureLoad(TextureRequestTable& requests, const char* filename) {
    TextureLoadRequest* request = &TableOperations::addToTable(requests).get<0>();
    request->mFileName = filename;
    request->mImageID = std::hash<std::string>()(request->mFileName);
    return request->mImageID;
  }

  SceneState::State _initRequestAssets(GameDatabase& db) {
    TextureRequestTable& textureRequests = std::get<TextureRequestTable>(db.mTables);

    auto& globals = std::get<GlobalGameData>(db.mTables);
    SceneState& scene = std::get<0>(globals.mRows).at();
    const std::string& root = std::get<SharedRow<FileSystem>>(globals.mRows).at().mRoot;
    scene.mBackgroundImage = _requestTextureLoad(textureRequests, (root + "background.png").c_str());
    scene.mPlayerImage = _requestTextureLoad(textureRequests, (root + "player.png").c_str());

    StaticGameObjectTable& staticObjects = std::get<StaticGameObjectTable>(db.mTables);
    GameObjectTable& gameobjects = std::get<GameObjectTable>(db.mTables);
    PlayerTable& players = std::get<PlayerTable>(db.mTables);
    std::get<SharedRow<TextureReference>>(players.mRows).at().mId = scene.mPlayerImage;
    //Make all the objects use the background image as their texture
    std::get<SharedRow<TextureReference>>(gameobjects.mRows).at().mId = scene.mBackgroundImage;
    std::get<SharedRow<TextureReference>>(staticObjects.mRows).at().mId = scene.mBackgroundImage;

    return SceneState::State::InitAwaitingAssets;
  }

  SceneState::State _awaitAssetLoading(GameDatabase& db) {
    //If there are any in progress requests keep waiting
    bool any = false;
    Queries::viewEachRow<Row<TextureLoadRequest>>(db, [&any](const Row<TextureLoadRequest>& requests) {
      for(const TextureLoadRequest& r : requests.mElements) {
        if(r.mStatus == RequestStatus::InProgress) {
          any = true;
        }
        else if(r.mStatus == RequestStatus::Failed) {
          printf("falied to load texture %s", r.mFileName.c_str());
        }
      }
    });

    //If any requests are pending, keep waiting
    if(any) {
      return SceneState::State::InitAwaitingAssets;
    }
    //If they're all done, clear them and continue on to the next phase
    //TODO: clear all tables containing row instead?
    TableOperations::resizeTable(std::get<TextureRequestTable>(db.mTables), 0);

    return SceneState::State::SetupScene;
  }

  void _updatePlayerInput(PlayerTable& players, GlobalPointForceTable& pointForces) {
    PROFILE_SCOPE("simulation", "playerinput");
    for(size_t i = 0; i < TableOperations::size(players); ++i) {
      PlayerInput& input = std::get<Row<PlayerInput>>(players.mRows).at(i);
      glm::vec2 move(input.mMoveX, input.mMoveY);
      const float speed = 0.05f;
      move *= speed;

      float& vx = std::get<FloatRow<LinVel, X>>(players.mRows).at(i);
      float& vy = std::get<FloatRow<LinVel, Y>>(players.mRows).at(i);
      glm::vec2 velocity(vx, vy);

      const float maxStoppingForce = 0.05f;
      //Apply a stopping force if there is no input. This is a flat amount so it doesn't negate physics
      const float epsilon = 0.0001f;
      const float velocityLen2 = glm::length2(velocity);
      if(glm::length2(move) < epsilon && velocityLen2 > epsilon) {
        //Apply an impulse in the opposite direction of velocity up to maxStoppingForce without exceeding velocity
        const float velocityLen = std::sqrt(velocityLen2);
        const float stoppingAmount = std::min(maxStoppingForce, velocityLen);
        const float stoppingMultiplier = stoppingAmount/velocityLen;
        velocity -= velocity*stoppingMultiplier;
      }
      //Apply an impulse in the desired move direction
      else {
        velocity += move;
      }

      vx = velocity.x;
      vy = velocity.y;

      if(input.mAction1) {
        input.mAction1 = false;
        const size_t lifetime = 5;
        const float strength = 0.05f;
        const size_t f = TableOperations::size(pointForces);
        TableOperations::addToTable(pointForces);
        std::get<FloatRow<Tags::Pos, Tags::X>>(pointForces.mRows).at(f) = std::get<FloatRow<Tags::Pos, Tags::X>>(players.mRows).at(i);
        std::get<FloatRow<Tags::Pos, Tags::Y>>(pointForces.mRows).at(f) = std::get<FloatRow<Tags::Pos, Tags::Y>>(players.mRows).at(i);
        std::get<ForceData::Strength>(pointForces.mRows).at(f) = strength;
        std::get<ForceData::Lifetime>(pointForces.mRows).at(f) = lifetime;
      }
    }
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

  void _updateDebugCamera(GameDatabase& db, CameraTable& cameras) {
    PROFILE_SCOPE("simulation", "debugcamera");
    constexpr const char* snapshotFilename = "debug.snap";
    bool loadSnapshot = false;
    for(size_t i = 0; i < TableOperations::size(cameras); ++i) {
      DebugCameraControl& input = std::get<Row<DebugCameraControl>>(cameras.mRows).at(i);
      const float speed = 0.3f;
      float& zoom = std::get<Row<Camera>>(cameras.mRows).at(i).zoom;
      zoom = std::max(0.0f, zoom + input.mAdjustZoom * speed);
      loadSnapshot = loadSnapshot || input.mLoadSnapshot;
      input.mLoadSnapshot = false;
      if(input.mTakeSnapshot) {
        Simulation::writeSnapshot(db, snapshotFilename);
      }
      input.mTakeSnapshot = false;
    }
    if(loadSnapshot) {
      Simulation::loadFromSnapshot(db, snapshotFilename);
    }
  }

  void _enforceWorldBoundary(GameDatabase& db) {
    PROFILE_SCOPE("simulation", "boundary");
    SceneState& scene = std::get<0>(std::get<GlobalGameData>(db.mTables).mRows).at();
    const glm::vec2 boundaryMin = scene.mBoundaryMin;
    const glm::vec2 boundaryMax = scene.mBoundaryMax;
    const float boundarySpringConstant = 1.0f*0.01f;
    Queries::viewEachRow<FloatRow<Pos, X>,
      FloatRow<LinVel, X>>(db,
        [&](FloatRow<Pos, X>& pos, FloatRow<LinVel, X>& linVel) {
      ispc::repelWorldBoundary(pos.mElements.data(), linVel.mElements.data(), boundaryMin.x, boundaryMax.x, boundarySpringConstant, (uint32_t)pos.mElements.size());
    });
    Queries::viewEachRow<FloatRow<Pos, Y>,
      FloatRow<LinVel, Y>>(db,
        [&](FloatRow<Pos, Y>& pos, FloatRow<LinVel, Y>& linVel) {
      ispc::repelWorldBoundary(pos.mElements.data(), linVel.mElements.data(), boundaryMin.y, boundaryMax.y, boundarySpringConstant, (uint32_t)pos.mElements.size());
    });
  }

  SceneState::State _update(GameDatabase& db) {
    _updatePlayerInput(std::get<PlayerTable>(db.mTables), std::get<GlobalPointForceTable>(db.mTables));
    _updateDebugCamera(db, std::get<CameraTable>(db.mTables));

    Simulation::_checkFragmentGoals(std::get<GameObjectTable>(db.mTables));
    Simulation::_migrateCompletedFragments(std::get<GameObjectTable>(db.mTables), std::get<StaticGameObjectTable>(db.mTables), std::get<BroadphaseTable>(db.mTables), _getStableMappings(db));

    _enforceWorldBoundary(db);
    _applyForces(std::get<GlobalPointForceTable>(db.mTables), std::get<GameObjectTable>(db.mTables));
    _tickForceLifetimes(std::get<GlobalPointForceTable>(db.mTables));

    Simulation::_updatePhysics(db, _getPhysicsConfig());

    return SceneState::State::Update;
  }
}

//Check to see if each fragment has reached its goal
void Simulation::_checkFragmentGoals(GameObjectTable& fragments) {
  PROFILE_SCOPE("simulation", "fragmentgoals");
  ispc::UniformConstVec2 pos = _unwrapConstFloatRow<Pos>(fragments);
  ispc::UniformConstVec2 goal = _unwrapConstFloatRow<FragmentGoal>(fragments);
  uint8_t* goalFound = _unwrapRow<FragmentGoalFoundRow>(fragments);
  const float minDistance = 0.5f;

  ispc::checkFragmentGoals(pos, goal, goalFound, minDistance, TableOperations::size(fragments));
}

void Simulation::_checkFragmentGoals(GameDatabase& db) {
  Simulation::_checkFragmentGoals(std::get<GameObjectTable>(db.mTables));
}

void Simulation::_migrateCompletedFragments(GameDatabase& db) {
  Simulation::_migrateCompletedFragments(std::get<GameObjectTable>(db.mTables), std::get<StaticGameObjectTable>(db.mTables), std::get<BroadphaseTable>(db.mTables), _getStableMappings(db));
}

void Simulation::_migrateCompletedFragments(GameObjectTable& fragments, StaticGameObjectTable& destinationFragments, BroadphaseTable& broadphase, StableElementMappings& mappings) {
  PROFILE_SCOPE("simulation", "migratefragments");
  //If the goal was found, move them to the destination table.
  //Do this in reverse so the swap remove doesn't mess up a previous removal
  const size_t oldTableSize = TableOperations::size(fragments);
  const size_t oldDestinationEnd = TableOperations::size(destinationFragments);
  uint8_t* goalFound = _unwrapRow<FragmentGoalFoundRow>(fragments);
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

    auto config = _getStaticBoundariesConfig();

    SweepNPruneBroadphase::recomputeBoundaries(
      _unwrapRow<SweepNPruneBroadphase::OldMinX>(tempTable),
      _unwrapRow<SweepNPruneBroadphase::OldMaxX>(tempTable),
      _unwrapRow<SweepNPruneBroadphase::NewMinX>(tempTable),
      _unwrapRow<SweepNPruneBroadphase::NewMaxX>(tempTable),
      posX.mElements.data() + oldDestinationEnd,
      config,
      std::get<SweepNPruneBroadphase::NeedsReinsert>(tempTable.mRows));
    SweepNPruneBroadphase::recomputeBoundaries(
      _unwrapRow<SweepNPruneBroadphase::OldMinY>(tempTable),
      _unwrapRow<SweepNPruneBroadphase::OldMaxY>(tempTable),
      _unwrapRow<SweepNPruneBroadphase::NewMinY>(tempTable),
      _unwrapRow<SweepNPruneBroadphase::NewMaxY>(tempTable),
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

SceneState::State Simulation::_setupScene(GameDatabase& db, const SceneArgs& args) {
  std::get<SharedRow<PhysicsTableIds>>(std::get<GlobalGameData>(db.mTables).mRows).at() = Simulation::_getPhysicsTableIds();

  std::random_device device;
  std::mt19937 generator(device());

  GameObjectTable& gameobjects = std::get<GameObjectTable>(db.mTables);
  SceneState& scene = std::get<0>(std::get<GlobalGameData>(db.mTables).mRows).at();

  CameraTable& camera = std::get<CameraTable>(db.mTables);
  TableOperations::addToTable(camera);
  Camera& mainCamera = std::get<Row<Camera>>(camera.mRows).at(0);
  mainCamera.zoom = 15.f;

  PlayerTable& players = std::get<PlayerTable>(db.mTables);
  TableOperations::stableResizeTable(players, UnpackedDatabaseElementID::fromPacked(GameDatabase::getTableIndex<PlayerTable>()), 1, _getStableMappings(db));
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
  TableOperations::stableResizeTable(gameobjects, UnpackedDatabaseElementID::fromPacked(GameDatabase::getTableIndex<GameObjectTable>()), total, _getStableMappings(db));

  float startX = -float(columns)/2.0f;
  float startY = -float(rows)/2.0f;
  float scale = 1.0f/float(rows);
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
    sprite.uMax = sprite.uMin + scale;
    sprite.vMax = sprite.vMin + scale;

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

  _initialPopulateBroadphase(db);

  return SceneState::State::Update;
}

void Simulation::_initialPopulateBroadphase(GameDatabase& db) {
  auto& broadphase = std::get<BroadphaseTable>(db.mTables);
  Queries::viewEachRow<
    FloatRow<Pos, X>,
    FloatRow<Pos, Y>,
    SweepNPruneBroadphase::OldMinX,
    SweepNPruneBroadphase::OldMinY,
    SweepNPruneBroadphase::OldMaxX,
    SweepNPruneBroadphase::OldMaxY,
    SweepNPruneBroadphase::NewMinX,
    SweepNPruneBroadphase::NewMinY,
    SweepNPruneBroadphase::NewMaxX,
    SweepNPruneBroadphase::NewMaxY,
    SweepNPruneBroadphase::NeedsReinsert,
    SweepNPruneBroadphase::Key>(db,
      [&](
    FloatRow<Pos, X>& posX,
    FloatRow<Pos, Y>& posY,
    SweepNPruneBroadphase::OldMinX& oldMinX,
    SweepNPruneBroadphase::OldMinY& oldMinY,
    SweepNPruneBroadphase::OldMaxX& oldMaxX,
    SweepNPruneBroadphase::OldMaxY& oldMaxY,
    SweepNPruneBroadphase::NewMinX& newMinX,
    SweepNPruneBroadphase::NewMinY& newMinY,
    SweepNPruneBroadphase::NewMaxX& newMaxX,
    SweepNPruneBroadphase::NewMaxY& newMaxY,
    SweepNPruneBroadphase::NeedsReinsert& needsReinsert,
    SweepNPruneBroadphase::Key& key) {

    auto config = _getBoundariesConfig();
    SweepNPruneBroadphase::recomputeBoundaries(oldMinX.mElements.data(), oldMaxX.mElements.data(), newMinX.mElements.data(), newMaxX.mElements.data(), posX.mElements.data(), config, needsReinsert);
    SweepNPruneBroadphase::recomputeBoundaries(oldMinY.mElements.data(), oldMaxY.mElements.data(), newMinY.mElements.data(), newMaxY.mElements.data(), posY.mElements.data(), config, needsReinsert);
    //These values were set by recomputeBoundaries but don't matter for the initial insert, reset them
    std::fill(needsReinsert.begin(), needsReinsert.end(), uint8_t(0));

    SweepNPruneBroadphase::insertRange(size_t(0), oldMinX.size(),
      broadphase,
      oldMinX,
      oldMinY,
      oldMaxX,
      oldMaxY,
      newMinX,
      newMinY,
      newMaxX,
      newMaxY,
      key);
  });
}

void Simulation::loadFromSnapshot(GameDatabase& db, const char* snapshotFilename) {
  if(std::basic_ifstream<uint8_t> stream(snapshotFilename, std::ios::binary); stream.good()) {
    DeserializeStream s(stream.rdbuf());
    Serializer<GameDatabase>::deserialize(s, db);
  }
}

void Simulation::snapshotInitGraphics(GameDatabase& db) {
  //Synchronously do the asset loading steps now which will load the assets and update any existing objects to point at them
  while(_initRequestAssets(db) != SceneState::State::InitAwaitingAssets) {}
  while(_awaitAssetLoading(db) != SceneState::State::SetupScene) {}
}

void Simulation::writeSnapshot(GameDatabase& db, const char* snapshotFilename) {
  if(std::basic_ofstream<uint8_t> stream(snapshotFilename, std::ios::binary); stream.good()) {
    SerializeStream s(stream.rdbuf());
    Serializer<GameDatabase>::serialize(db, s);
  }
}

void Simulation::update(GameDatabase& db) {
  PROFILE_SCOPE("simulation", "update");
  constexpr bool enableDebugSnapshot = false;
  if(enableDebugSnapshot) {
    PROFILE_SCOPE("simulation", "snapshot");
    writeSnapshot(db, "recovery.snap");
  }

  GlobalGameData& globals = std::get<GlobalGameData>(db.mTables);
  SceneState& sceneState = std::get<0>(globals.mRows).at();
  SceneState::State newState = sceneState.mState;
  switch(sceneState.mState) {
    case SceneState::State::InitRequestAssets:
      newState = _initRequestAssets(db);
      break;
    case SceneState::State::InitAwaitingAssets:
      newState = _awaitAssetLoading(db);
      break;
    case SceneState::State::SetupScene: {
      SceneArgs args;
      args.mFragmentRows = 4;
      args.mFragmentColumns = 4;
      newState = _setupScene(db, args);
      break;
    }
    case SceneState::State::Update:
      newState = _update(db);
      break;
  }
  sceneState.mState = newState;
}

void Simulation::_updatePhysics(GameDatabase& db, const PhysicsConfig& config) {
  PROFILE_SCOPE("physics", "update");
  using PosX = FloatRow<Pos, X>;
  using PosY = FloatRow<Pos, Y>;
  using LinVelX = FloatRow<LinVel, X>;
  using LinVelY = FloatRow<LinVel, Y>;
  using AngVel = FloatRow<AngVel, Angle>;
  using RotX = FloatRow<Rot, CosAngle>;
  using RotY = FloatRow<Rot, SinAngle>;
  //For now use the existence of this row to indicate that the given object should participate in collision
  using HasCollision = Row<CubeSprite>;

  auto& broadphase = std::get<BroadphaseTable>(db.mTables);
  auto& collisionPairs = std::get<CollisionPairsTable>(db.mTables);
  auto& constraints = std::get<ConstraintsTable>(db.mTables);
  auto& staticConstraints = std::get<ContactConstraintsToStaticObjectsTable>(db.mTables);
  auto& constraintsCommon = std::get<ConstraintCommonTable>(db.mTables);
  auto& globals = std::get<GlobalGameData>(db.mTables);
  const PhysicsTableIds& physicsTables = std::get<SharedRow<PhysicsTableIds>>(globals.mRows).at();
  ConstraintsTableMappings& constraintsMappings = std::get<SharedRow<ConstraintsTableMappings>>(globals.mRows).at();

  const float linearDragMultiplier = 0.96f;
  {
    PROFILE_SCOPE("physics", "lineardamping");
    Physics::applyDampingMultiplier<LinVelX, LinVelY>(db, linearDragMultiplier);
  }
  const float angularDragMultiplier = 0.99f;
  {
    PROFILE_SCOPE("physics", "angulardamping");
    Physics::details::applyDampingMultiplierAxis<AngVel>(db, angularDragMultiplier);
  }
  SweepNPruneBroadphase::ChangedCollisionPairs& changedPairs = std::get<SharedRow<SweepNPruneBroadphase::ChangedCollisionPairs>>(broadphase.mRows).at();

  Queries::viewEachRow<SweepNPruneBroadphase::OldMinX,
    SweepNPruneBroadphase::OldMinY,
    SweepNPruneBroadphase::OldMaxX,
    SweepNPruneBroadphase::OldMaxY,
    SweepNPruneBroadphase::NewMinX,
    SweepNPruneBroadphase::NewMinY,
    SweepNPruneBroadphase::NewMaxX,
    SweepNPruneBroadphase::NewMaxY,
    SweepNPruneBroadphase::NeedsReinsert,
    FloatRow<Pos, X>,
    FloatRow<Pos, Y>,
    SweepNPruneBroadphase::Key>(db,
      [&](
    SweepNPruneBroadphase::OldMinX& oldMinX,
    SweepNPruneBroadphase::OldMinY& oldMinY,
    SweepNPruneBroadphase::OldMaxX& oldMaxX,
    SweepNPruneBroadphase::OldMaxY& oldMaxY,
    SweepNPruneBroadphase::NewMinX& newMinX,
    SweepNPruneBroadphase::NewMinY& newMinY,
    SweepNPruneBroadphase::NewMaxX& newMaxX,
    SweepNPruneBroadphase::NewMaxY& newMaxY,
    SweepNPruneBroadphase::NeedsReinsert& needsReinsert,
    FloatRow<Pos, X>& posX,
    FloatRow<Pos, Y>& posY,
    SweepNPruneBroadphase::Key& key) {
    PROFILE_SCOPE("physics", "broadphase");

    auto config = _getBoundariesConfig();
    const bool needsUpdateX = SweepNPruneBroadphase::recomputeBoundaries(oldMinX.mElements.data(), oldMaxX.mElements.data(), newMinX.mElements.data(), newMaxX.mElements.data(), posX.mElements.data(), config, needsReinsert);
    const bool needsUpdateY = SweepNPruneBroadphase::recomputeBoundaries(oldMinY.mElements.data(), oldMaxY.mElements.data(), newMinY.mElements.data(), newMaxY.mElements.data(), posY.mElements.data(), config, needsReinsert);

    if(needsUpdateX || needsUpdateY) {
      SweepNPruneBroadphase::reinsertRangeAsNeeded(needsReinsert,
        broadphase,
        oldMinX,
        oldMinY,
        oldMaxX,
        oldMaxY,
        newMinX,
        newMinY,
        newMaxX,
        newMaxY,
        key);
    }

    SweepNPruneBroadphase::updateCollisionPairs<CollisionPairIndexA, CollisionPairIndexB, GameDatabase>(
      std::get<SharedRow<SweepNPruneBroadphase::PairChanges>>(broadphase.mRows).at(),
      std::get<SharedRow<SweepNPruneBroadphase::CollisionPairMappings>>(broadphase.mRows).at(),
      collisionPairs,
      physicsTables,
      _getStableMappings(db),
      changedPairs);
  });

  {
    PROFILE_SCOPE("physics", "fill narrowphase");
    Physics::fillNarrowphaseData<PosX, PosY, RotX, RotY>(collisionPairs, db, _getStableMappings(db), physicsTables);
  }

  {
    PROFILE_SCOPE("physics", "generate contacts");
    Physics::generateContacts(collisionPairs);
  }

  auto& debug = std::get<DebugLineTable>(db.mTables);
  auto addLine = [&debug](glm::vec2 a, glm::vec2 b, glm::vec3 color) {
    DebugLineTable::ElementRef e = TableOperations::addToTable(debug);
    e.get<0>().mPos = a;
    e.get<0>().mColor = color;
    e = TableOperations::addToTable(debug);
    e.get<0>().mPos = b;
    e.get<0>().mColor = color;
  };

  static bool drawCollisionPairs = false;
  static bool drawContacts = false;
  if(drawCollisionPairs) {
    auto& ax = std::get<NarrowphaseData<PairA>::PosX>(collisionPairs.mRows);
    auto& ay = std::get<NarrowphaseData<PairA>::PosY>(collisionPairs.mRows);
    auto& bx = std::get<NarrowphaseData<PairB>::PosX>(collisionPairs.mRows);
    auto& by = std::get<NarrowphaseData<PairB>::PosY>(collisionPairs.mRows);
    for(size_t i = 0; i < ax.size(); ++i) {
      addLine(glm::vec2(ax.at(i), ay.at(i)), glm::vec2(bx.at(i), by.at(i)), glm::vec3(0.0f, 1.0f, 0.0f));
    }
  }

  {
    PROFILE_SCOPE("physics", "build constraints table");
    ConstraintsTableBuilder::build(db, changedPairs, _getStableMappings(db), constraintsMappings, physicsTables, config);
  }
  {
    PROFILE_SCOPE("physics", "fill constraint velocities");
    Physics::fillConstraintVelocities<LinVelX, LinVelY, AngVel>(constraintsCommon, db);
  }
  {
    PROFILE_SCOPE("physics", "setup constraints");
    Physics::setupConstraints(constraints, staticConstraints);
  }

  if(drawContacts) {
    for(size_t i = 0; i < TableOperations::size(collisionPairs); ++i) {
      CollisionPairsTable::ElementRef e = TableOperations::getElement(collisionPairs, i);
      float overlapOne = e.get<12>();
      float overlapTwo = e.get<15>();
      glm::vec2 posA{ e.get<2>(), e.get<3>() };
      glm::vec2 posB{ e.get<6>(), e.get<7>() };
      glm::vec2 contactOne{ e.get<10>(), e.get<11>() };
      glm::vec2 contactTwo{ e.get<13>(), e.get<14>() };
      glm::vec2 normal{ e.get<16>(), e.get<17>() };
      if(overlapOne >= 0.0f) {
        addLine(posA, contactOne, glm::vec3(1.0f, 0.0f, 0.0f));
        addLine(contactOne, contactOne + normal*0.25f, glm::vec3(0.0f, 1.0f, 0.0f));
        addLine(contactOne, contactOne + normal*overlapOne, glm::vec3(1.0f, 1.0f, 0.0f));
      }
      if(overlapTwo >= 0.0f) {
        addLine(posA, contactTwo, glm::vec3(1.0f, 0.0f, 1.0f));
        addLine(contactTwo, contactTwo + normal*0.25f, glm::vec3(0.0f, 1.0f, 1.0f));
        addLine(contactTwo, contactTwo + normal*overlapTwo, glm::vec3(1.0f, 1.0f, 1.0f));
      }
    }
  }

  const int solveIterations = 5;
  //TODO: stop early if global lambda sum falls below tolerance
  for(int i = 0; i < solveIterations; ++i) {
    PROFILE_SCOPE("physics", "solve constraints");
    Physics::solveConstraints(constraints, staticConstraints, constraintsCommon, config);
  }

  {
    PROFILE_SCOPE("physics", "store constraint velocities");
    Physics::storeConstraintVelocities<LinVelX, LinVelY, AngVel>(constraintsCommon, db);
  }

  {
    PROFILE_SCOPE("physics", "integrate position");
    Physics::integratePosition<LinVelX, LinVelY, PosX, PosY>(db);
  }
  {
    PROFILE_SCOPE("physics", "integrate rotation");
    Physics::integrateRotation<RotX, RotY, AngVel>(db);
  }
}

PhysicsTableIds Simulation::_getPhysicsTableIds() {
  PhysicsTableIds physicsTables;
  physicsTables.mTableIDMask = GameDatabase::ElementID::TABLE_INDEX_MASK;
  physicsTables.mSharedMassConstraintTable = GameDatabase::getTableIndex<ConstraintsTable>().mValue;
  physicsTables.mZeroMassConstraintTable = GameDatabase::getTableIndex<ContactConstraintsToStaticObjectsTable>().mValue;
  physicsTables.mSharedMassObjectTable = GameDatabase::getTableIndex<GameObjectTable>().mValue;
  physicsTables.mZeroMassObjectTable = GameDatabase::getTableIndex<StaticGameObjectTable>().mValue;
  physicsTables.mConstriantsCommonTable = GameDatabase::getTableIndex<ConstraintCommonTable>().mValue;
  physicsTables.mElementIDMask = GameDatabase::ElementID::ELEMENT_INDEX_MASK;
  return physicsTables;
}
