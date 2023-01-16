#include "Precompile.h"
#include "Simulation.h"

#include "Queries.h"

#include "unity.h"

#include "glm/gtx/norm.hpp"
#include <random>

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

  SweepNPruneBroadphase::BoundariesConfig _getBoundariesConfig() {
    return {};
  }

  SweepNPruneBroadphase::BoundariesConfig _getStaticBoundariesConfig() {
    //Can fit more snugly since they are axis aligned
    SweepNPruneBroadphase::BoundariesConfig result;
    result.mPadding = 0.0f;
    return result;
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

  SceneState::State _setupScene(GameDatabase& db) {
    std::get<SharedRow<PhysicsTableIds>>(std::get<GlobalGameData>(db.mTables).mRows).at() = Simulation::_getPhysicsTableIds();

    std::random_device device;
    std::mt19937 generator(device());

    StaticGameObjectTable& staticObjects = std::get<StaticGameObjectTable>(db.mTables);
    GameObjectTable& gameobjects = std::get<GameObjectTable>(db.mTables);
    SceneState& scene = std::get<0>(std::get<GlobalGameData>(db.mTables).mRows).at();

    CameraTable& camera = std::get<CameraTable>(db.mTables);
    TableOperations::addToTable(camera);
    Camera& mainCamera = std::get<Row<Camera>>(camera.mRows).at(0);
    mainCamera.zoom = 15.f;

    PlayerTable& players = std::get<PlayerTable>(db.mTables);
    TableOperations::resizeTable(players, 1);
    std::get<SharedRow<TextureReference>>(players.mRows).at().mId = scene.mPlayerImage;
    std::get<FloatRow<Rot, CosAngle>>(players.mRows).at(0) = 1.0f;
    //Random angle in sort of radians
    const float playerStartAngle = float(generator() % 360)*6.282f/360.0f;
    const float playerStartDistance = 25.0f;
    //Start way off the screen, the world boundary will fling them into the scene
    std::get<FloatRow<Pos, X>>(players.mRows).at(0) = playerStartDistance*std::cos(playerStartAngle);
    std::get<FloatRow<Pos, Y>>(players.mRows).at(0) = playerStartDistance*std::sin(playerStartAngle);

    //Make all the objects use the background image as their texture
    std::get<SharedRow<TextureReference>>(gameobjects.mRows).at().mId = scene.mBackgroundImage;
    std::get<SharedRow<TextureReference>>(staticObjects.mRows).at().mId = scene.mBackgroundImage;

    //Add some arbitrary objects for testing
    const size_t rows = 5;
    const size_t columns = 5;
    const size_t total = rows*columns;
    TableOperations::resizeTable(gameobjects, total);

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

    auto& broadphase = std::get<BroadphaseTable>(db.mTables);
    Queries::viewEachRowWithTableID<SweepNPruneBroadphase::OldMinX,
      SweepNPruneBroadphase::OldMinY,
      SweepNPruneBroadphase::OldMaxX,
      SweepNPruneBroadphase::OldMaxY,
      SweepNPruneBroadphase::NewMinX,
      SweepNPruneBroadphase::NewMinY,
      SweepNPruneBroadphase::NewMaxX,
      SweepNPruneBroadphase::NewMaxY,
      SweepNPruneBroadphase::NeedsReinsert,
      SweepNPruneBroadphase::Key>(db,
        [&](GameDatabase::ElementID tableID,
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

      SweepNPruneBroadphase::insertRange(tableID.mValue, size_t(0), oldMinX.size(),
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

    return SceneState::State::Update;
  }

  void _updatePlayerInput(PlayerTable& players) {
    for(size_t i = 0; i < TableOperations::size(players); ++i) {
      const PlayerInput& input = std::get<Row<PlayerInput>>(players.mRows).at(i);
      glm::vec2 move(input.mMoveX, input.mMoveY);
      const float speed = 0.05f;
      move *= speed;

      float& vx = std::get<FloatRow<LinVel, X>>(players.mRows).at(i);
      float& vy = std::get<FloatRow<LinVel, Y>>(players.mRows).at(i);
      //Debug hack to slowly rotate player
      //std::get<FloatRow<AngVel, Angle>>(players.mRows).at(i) = input.mAction1 ? 0.01f : 0.0f;
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
    }
  }

  void _updateDebugCamera(CameraTable& cameras) {
    for(size_t i = 0; i < TableOperations::size(cameras); ++i) {
      const DebugCameraControl& input = std::get<Row<DebugCameraControl>>(cameras.mRows).at(i);
      const float speed = 0.3f;
      float& zoom = std::get<Row<Camera>>(cameras.mRows).at(i).zoom;
      zoom = std::max(0.0f, zoom + input.mAdjustZoom * speed);
    }
  }

  //Check to see if each fragment has reached its goal
  void _checkFragmentGoals(GameObjectTable& fragments, StaticGameObjectTable& destinationFragments, BroadphaseTable& broadphase) {
    ispc::UniformConstVec2 pos = _unwrapConstFloatRow<Pos>(fragments);
    ispc::UniformConstVec2 goal = _unwrapConstFloatRow<FragmentGoal>(fragments);
    uint8_t* goalFound = _unwrapRow<FragmentGoalFoundRow>(fragments);
    const float minDistance = 0.5f;

    ispc::checkFragmentGoals(pos, goal, goalFound, minDistance, TableOperations::size(fragments));

    //If the goal was found, move them to the destination table.
    //Do this in reverse so the swap remove doesn't mess up a previous removal
    const size_t oldTableSize = TableOperations::size(fragments);
    const size_t oldDestinationEnd = TableOperations::size(destinationFragments);

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

        TableOperations::migrateOne(fragments, destinationFragments, reverseIndex);
        //Update the mapping for the element that got swapped into this location
        if(reverseIndex < TableOperations::size(fragments)) {
          SweepNPruneBroadphase::informObjectMovedTables(
            std::get<SharedRow<SweepNPruneBroadphase::CollisionPairMappings>>(broadphase.mRows).at(),
            std::get<SharedRow<SweepNPruneBroadphase::PairChanges>>(broadphase.mRows).at(),
            keys.at(reverseIndex),
            reverseIndex);
        }
      }
    }

    //Update the broadphase mappings in place here
    //If this becomes a more common occurence this table migration should be deferred so this update
    //can be done in a less fragile way
    const size_t newElements = TableOperations::size(destinationFragments) - oldDestinationEnd;
    if(newElements > 0) {
      auto& posX = std::get<FloatRow<Pos, X>>(destinationFragments.mRows);
      auto& posY = std::get<FloatRow<Pos, Y>>(destinationFragments.mRows);
      auto& keys = std::get<SweepNPruneBroadphase::Key>(destinationFragments.mRows);
      //Scratch containers used to insert the elements, don't need to be stored since objects won't move after this
      Table<SweepNPruneBroadphase::NeedsReinsert,
        SweepNPruneBroadphase::OldMinX,
        SweepNPruneBroadphase::OldMinY,
        SweepNPruneBroadphase::OldMaxX,
        SweepNPruneBroadphase::OldMaxY,
        SweepNPruneBroadphase::NewMinX,
        SweepNPruneBroadphase::NewMinY,
        SweepNPruneBroadphase::NewMaxX,
        SweepNPruneBroadphase::NewMaxY,
        SweepNPruneBroadphase::Key> tempTable;
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

      //Table id with built in offset to the start of the index in the destination table, which is different from the temp table
      SweepNPruneBroadphase::insertRange(GameDatabase::getTableIndex<StaticGameObjectTable>().mValue + oldDestinationEnd,
        0,
        newElements,
        broadphase,
        std::get<SweepNPruneBroadphase::OldMinX>(tempTable.mRows),
        std::get<SweepNPruneBroadphase::OldMinY>(tempTable.mRows),
        std::get<SweepNPruneBroadphase::OldMaxX>(tempTable.mRows),
        std::get<SweepNPruneBroadphase::OldMaxY>(tempTable.mRows),
        std::get<SweepNPruneBroadphase::NewMinX>(tempTable.mRows),
        std::get<SweepNPruneBroadphase::NewMinY>(tempTable.mRows),
        std::get<SweepNPruneBroadphase::NewMaxX>(tempTable.mRows),
        std::get<SweepNPruneBroadphase::NewMaxY>(tempTable.mRows),
        std::get<SweepNPruneBroadphase::Key>(tempTable.mRows));

      //Copy the keys from the temp table to the destination table
      std::memcpy(keys.mElements.data() + oldDestinationEnd, std::get<SweepNPruneBroadphase::Key>(tempTable.mRows).mElements.data(), sizeof(size_t)*newElements);
    }
  }

  void _enforceWorldBoundary(GameDatabase& db) {
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

  void _updatePhysics(GameDatabase& db) {
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
    const PhysicsTableIds& physicsTables = std::get<SharedRow<PhysicsTableIds>>(std::get<GlobalGameData>(db.mTables).mRows).at();

    const float linearDragMultiplier = 0.96f;
    Physics::applyDampingMultiplier<LinVelX, LinVelY>(db, linearDragMultiplier);
    const float angularDragMultiplier = 0.99f;
    Physics::details::applyDampingMultiplierAxis<AngVel>(db, angularDragMultiplier);

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

      SweepNPruneBroadphase::updateCollisionPairs<CollisionPairIndexA, CollisionPairIndexB>(
        std::get<SharedRow<SweepNPruneBroadphase::PairChanges>>(broadphase.mRows).at(),
        std::get<SharedRow<SweepNPruneBroadphase::CollisionPairMappings>>(broadphase.mRows).at(),
        collisionPairs,
        physicsTables);
    });

    Physics::fillNarrowphaseData<PosX, PosY, RotX, RotY>(collisionPairs, db);

    Physics::generateContacts(collisionPairs);

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

    Physics::buildConstraintsTable(collisionPairs, constraints, staticConstraints, constraintsCommon, physicsTables);
    Physics::fillConstraintVelocities<LinVelX, LinVelY, AngVel>(constraintsCommon, db);
    Physics::setupConstraints(constraints, staticConstraints);

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
      Physics::solveConstraints(constraints, staticConstraints, constraintsCommon);
    }

    Physics::storeConstraintVelocities<LinVelX, LinVelY, AngVel>(constraintsCommon, db);

    Physics::integratePosition<LinVelX, LinVelY, PosX, PosY>(db);
    Physics::integrateRotation<RotX, RotY, AngVel>(db);
  }

  SceneState::State _update(GameDatabase& db) {
    using namespace Tags;

    _updatePlayerInput(std::get<PlayerTable>(db.mTables));
    _updateDebugCamera(std::get<CameraTable>(db.mTables));

    _checkFragmentGoals(std::get<GameObjectTable>(db.mTables), std::get<StaticGameObjectTable>(db.mTables), std::get<BroadphaseTable>(db.mTables));

    _enforceWorldBoundary(db);
    _updatePhysics(db);

    return SceneState::State::Update;
  }
}

void Simulation::update(GameDatabase& db) {
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
    case SceneState::State::SetupScene:
      newState = _setupScene(db);
      break;
    case SceneState::State::Update:
      newState = _update(db);
      break;
  }
  sceneState.mState = newState;
}

PhysicsTableIds Simulation::_getPhysicsTableIds() {
  PhysicsTableIds physicsTables;
  physicsTables.mTableIDMask = GameDatabase::ElementID::TABLE_INDEX_MASK;
  physicsTables.mSharedMassTable = GameDatabase::getTableIndex<GameObjectTable>().mValue;
  physicsTables.mZeroMassTable = GameDatabase::getTableIndex<StaticGameObjectTable>().mValue;
  return physicsTables;
}
