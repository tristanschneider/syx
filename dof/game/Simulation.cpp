#include "Precompile.h"
#include "Simulation.h"

#include "Queries.h"

#include "out_ispc/unity.h"

#include "glm/gtx/norm.hpp"

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

  SceneState::State _setupScene(GameDatabase& db) {
    GameObjectTable& gameobjects = std::get<GameObjectTable>(db.mTables);
    const SceneState& scene = std::get<0>(std::get<GlobalGameData>(db.mTables).mRows).at();

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
    const size_t rows = 100;
    const size_t columns = 100;
    const size_t total = rows*columns;
    TableOperations::resizeTable(gameobjects, total);

    float startX = -float(columns)/2.0f;
    float startY = -float(rows)/2.0f;
    float scale = 1.0f/float(rows);
    for(size_t i = 0; i < total; ++i) {
      CubeSprite& sprite = std::get<Row<CubeSprite>>(gameobjects.mRows).at(i);
      const size_t row = i / columns;
      const size_t column = i % columns;
      sprite.uMin = float(column)/float(columns);
      sprite.vMin = float(row)/float(rows);
      sprite.uMax = sprite.uMin + scale;
      sprite.vMax = sprite.vMin + scale;

      std::get<FloatRow<Pos, X>>(gameobjects.mRows).at(i) = startX + sprite.uMin*float(columns);
      std::get<FloatRow<Pos, Y>>(gameobjects.mRows).at(i) = startY + sprite.vMin*float(rows);

      std::get<FloatRow<Rot, CosAngle>>(gameobjects.mRows).at(i) = 1.0f;
    }

    return SceneState::State::Update;
  }

  SceneState::State _update(GameDatabase& db) {
    using namespace Tags;

    PlayerTable& players = std::get<PlayerTable>(db.mTables);
    for(size_t i = 0; i < TableOperations::size(players); ++i) {
      const PlayerInput& input = std::get<Row<PlayerInput>>(players.mRows).at(i);
      glm::vec2 move(input.mMoveX, input.mMoveY);
      const float speed = 0.05f;
      move *= speed;

      float& vx = std::get<FloatRow<LinVel, X>>(players.mRows).at(i);
      float& vy = std::get<FloatRow<LinVel, Y>>(players.mRows).at(i);
      glm::vec2 velocity(vx, vy);

      const float maxStoppingForce = 0.1f;
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

    CameraTable& cameras = std::get<CameraTable>(db.mTables);
    for(size_t i = 0; i < TableOperations::size(cameras); ++i) {
      const DebugCameraControl& input = std::get<Row<DebugCameraControl>>(cameras.mRows).at(i);
      const float speed = 0.3f;
      float& zoom = std::get<Row<Camera>>(cameras.mRows).at(i).zoom;
      zoom = std::max(0.0f, zoom + input.mAdjustZoom * speed);
    }

    GameObjectTable& gameobjects = std::get<GameObjectTable>(db.mTables);
    for(size_t i = 0; i < TableOperations::size(gameobjects); ++i) {
      float* cosAngle = &std::get<FloatRow<Rot, CosAngle>>(gameobjects.mRows).at(i);
      float* sinAngle = &std::get<FloatRow<Rot, SinAngle>>(gameobjects.mRows).at(i);
      float angularImpulse = 0.01f * (i % 2 ? 1.0f : -1.f);
      ispc::integrateRotation(cosAngle, sinAngle, &angularImpulse, 1);
      float a = asin(*sinAngle);
      float b = acos(*cosAngle);
      a;b;
    }

    //World boundary
    const glm::vec2 boundaryMin(-30.0f, -50.0f);
    const glm::vec2 boundaryMax(50.0f, 50.0f);
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


    //Apply drag
    const float dragMultiplier = 0.95f;
    Queries::viewEachRow<FloatRow<LinVel, X>>(db,
        [&](FloatRow<LinVel, X>& linVel) {
      ispc::applyDampingMultiplier(linVel.mElements.data(), dragMultiplier, (uint32_t)linVel.mElements.size());
    });
    Queries::viewEachRow<FloatRow<LinVel, Y>>(db,
        [&](FloatRow<LinVel, Y>& linVel) {
      ispc::applyDampingMultiplier(linVel.mElements.data(), dragMultiplier, (uint32_t)linVel.mElements.size());
    });

    //Integrate position
    Queries::viewEachRow<FloatRow<Pos, X>,
      FloatRow<LinVel, X>>(db,
        [&](FloatRow<Pos, X>& pos, FloatRow<LinVel, X>& linVel) {
      ispc::integratePosition(pos.mElements.data(), linVel.mElements.data(), (uint32_t)pos.mElements.size());
    });
    Queries::viewEachRow<FloatRow<Pos, Y>,
      FloatRow<LinVel, Y>>(db,
        [&](FloatRow<Pos, Y>& pos, FloatRow<LinVel, Y>& linVel) {
      ispc::integratePosition(pos.mElements.data(), linVel.mElements.data(), (uint32_t)pos.mElements.size());
    });
    //Integrate rotation
    Queries::viewEachRow<FloatRow<Rot, CosAngle>,
      FloatRow<Rot, SinAngle>,
      FloatRow<AngVel, Angle>>(db,
        [&](FloatRow<Rot, CosAngle>& rotX, FloatRow<Rot, SinAngle>& rotY, FloatRow<AngVel, Angle>& velocity) {
      ispc::integrateRotation(rotX.mElements.data(), rotY.mElements.data(), velocity.mElements.data(), (uint32_t)rotX.mElements.size());
    });

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