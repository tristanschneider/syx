#include "Precompile.h"
#include "Simulation.h"

#include "Queries.h"

#include "out_ispc/unity.h"

#include "glm/gtx/norm.hpp"

extern std::vector<float> DEBUG_HACK;


namespace {
  using namespace Tags;
  size_t _requestTextureLoad(TextureRequestTable& requests, const char* filename) {
    TextureLoadRequest* request = &TableOperations::addToTable(requests).get<0>();
    request->mFileName = filename;
    request->mImageID = std::hash<std::string>()(request->mFileName);
    return request->mImageID;
  }

  SceneState::State _initRequestAssets(GameDatabase& db) {
    TextureRequestTable& textureRequests = std::get<TextureRequestTable>(db.mTables);

    SceneState& scene = std::get<0>(std::get<GlobalGameData>(db.mTables).mRows).at();
    scene.mBackgroundImage = _requestTextureLoad(textureRequests, "C:/syx/dof/data/background.png");
    scene.mPlayerImage = _requestTextureLoad(textureRequests, "C:/syx/dof/data/player.png");

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

  void _allocateBroadphaseFromScene(const SceneState& scene, BroadphaseTable& broadphase) {
    auto& requestedDimensions = std::get<SharedRow<GridBroadphase::RequestedDimensions>>(broadphase.mRows).at();
    const int broadphasePadding = 5;
    requestedDimensions.mMin.x = int(scene.mBoundaryMin.x) - broadphasePadding;
    requestedDimensions.mMin.y = int(scene.mBoundaryMin.y) - broadphasePadding;
    requestedDimensions.mMax.x = int(scene.mBoundaryMax.x) + broadphasePadding;
    requestedDimensions.mMax.y = int(scene.mBoundaryMax.y) + broadphasePadding;
    Physics::allocateBroadphase(broadphase);
  }

  SceneState::State _setupScene(GameDatabase& db) {
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

    //Make all the objects use the background image as their texture
    std::get<SharedRow<TextureReference>>(gameobjects.mRows).at().mId = scene.mBackgroundImage;

    //Add some arbitrary objects for testing
    const size_t rows = 1;
    const size_t columns = 1;
    const size_t total = rows*columns;
    TableOperations::resizeTable(gameobjects, total);

    float startX = -float(columns)/2.0f;
    float startY = -float(rows)/2.0f;
    float scale = 1.0f/float(rows);
    auto& posX = std::get<FloatRow<Pos, X>>(gameobjects.mRows);
    auto& posY = std::get<FloatRow<Pos, Y>>(gameobjects.mRows);
    for(size_t i = 0; i < total; ++i) {
      CubeSprite& sprite = std::get<Row<CubeSprite>>(gameobjects.mRows).at(i);
      const size_t row = i / columns;
      const size_t column = i % columns;
      sprite.uMin = float(column)/float(columns);
      sprite.vMin = float(row)/float(rows);
      sprite.uMax = sprite.uMin + scale;
      sprite.vMax = sprite.vMin + scale;

      posX.at(i) = startX + sprite.uMin*float(columns);
      posY.at(i) = startY + sprite.vMin*float(rows);

      std::get<FloatRow<Rot, CosAngle>>(gameobjects.mRows).at(i) = 1.0f;
    }

    const float boundaryPadding = 1.0f;
    const size_t first = 0;
    const size_t last = total - 1;
    scene.mBoundaryMin = glm::vec2(posX.at(first), posY.at(first)) - glm::vec2(boundaryPadding);
    scene.mBoundaryMax = glm::vec2(posX.at(last), posY.at(last)) + glm::vec2(boundaryPadding);

    _allocateBroadphaseFromScene(scene, std::get<BroadphaseTable>(db.mTables));

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

    const float dragMultiplier = 0.96f;
    Physics::applyDampingMultiplier<LinVelX, LinVelY>(db, dragMultiplier);

    //TODO: don't rebuild every frame
    Physics::clearBroadphase(broadphase);
    //Gather collision pairs in any table that is interested.
    //This passes the table id forward to the physics tables which is used during the fill/store functions
    //to figure out which table to refer to when moving data to/from physics
    Queries::viewEachRowWithTableID<PosX, PosY, HasCollision>(db,
      [&](GameDatabase::ElementID tableId, PosX& posX, PosY& posY, HasCollision&) {
        Physics::rebuildBroadphase(tableId.mValue, posX.mElements.data(), posY.mElements.data(), broadphase, posX.mElements.size());
    });

    Physics::generateCollisionPairs(broadphase, collisionPairs);

    Physics::fillNarrowphaseData<PosX, PosY, RotX, RotY>(collisionPairs, db);

    Physics::generateContacts(collisionPairs);

    Physics::buildConstraintsTable(collisionPairs, constraints);
    Physics::fillConstraintVelocities<LinVelX, LinVelY, AngVel>(constraints, db);
    Physics::setupConstraints(constraints);

    /*
    auto& debug = std::get<DebugLineTable>(db.mTables);
    auto addLine = [&debug](glm::vec2 a, glm::vec2 b, glm::vec3 color) {
      DebugLineTable::ElementRef e = TableOperations::addToTable(debug);
      e.get<0>().mPos = a;
      e.get<0>().mColor = color;
      e = TableOperations::addToTable(debug);
      e.get<0>().mPos = b;
      e.get<0>().mColor = color;
    };


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

    glm::vec2 first, last;
    bool anyFound = false;
    for(size_t i = 0; i + 1 < DEBUG_HACK.size(); i += 2) {
      glm::vec2 p{ DEBUG_HACK[i], DEBUG_HACK[i + 1] };
      if(std::abs(p.x - 1000.0f) < 0.001f) {
        break;
      }
      anyFound = true;
      for(int j = 0; j < 1; ++j) {
        DebugLineTable::ElementRef e = TableOperations::addToTable(debug);
        e.get<0>().mPos = p;
        e.get<0>().mColor = glm::vec3(1.0f);
        if(!i) {
          first = p;
          break;
        }
      }
      last = p;
    }
    //if(anyFound) {
    //  addLine(first, last, glm::vec3(1.0f));
    //}
    */

    const int solveIterations = 5;
    //TODO: stop early if global lambda sum falls below tolerance
    for(int i = 0; i < solveIterations; ++i) {
      Physics::solveConstraints(constraints);
    }

    Physics::storeConstraintVelocities<LinVelX, LinVelY, AngVel>(constraints, db);

    Physics::integratePosition<LinVelX, LinVelY, PosX, PosY>(db);
    Physics::integrateRotation<RotX, RotY, AngVel>(db);
  }

  SceneState::State _update(GameDatabase& db) {
    using namespace Tags;

    _updatePlayerInput(std::get<PlayerTable>(db.mTables));
    _updateDebugCamera(std::get<CameraTable>(db.mTables));

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