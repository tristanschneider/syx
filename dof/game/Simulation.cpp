#include "Precompile.h"
#include "Simulation.h"

#include "Queries.h"

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

    PlayerTable& players = std::get<PlayerTable>(db.mTables);
    TableOperations::resizeTable(players, 1);
    std::get<SharedRow<TextureReference>>(players.mRows).at().mId = scene.mPlayerImage;

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
    }

    return SceneState::State::Update;
  }

  SceneState::State _update(GameDatabase& db) {
    using namespace Tags;

    PlayerTable& players = std::get<PlayerTable>(db.mTables);
    for(size_t i = 0; i < TableOperations::size(players); ++i) {
      const PlayerInput& input = std::get<Row<PlayerInput>>(players.mRows).at(i);
      //Normalize. Kind of odd to do it here instead of at input layer, works for the moment
      const float len = std::sqrt(input.mMoveX*input.mMoveX + input.mMoveY*input.mMoveY);
      float moveX = input.mMoveX;
      float moveY = input.mMoveY;
      const float speed = 0.05f;
      if(std::abs(len) > 0.0001f) {
        const float multiplier = speed/len;
        moveX *= multiplier;
        moveY *= multiplier;
      }

      std::get<FloatRow<Pos, X>>(players.mRows).at(i) += moveX;
      std::get<FloatRow<Pos, Y>>(players.mRows).at(i) += moveY;
    }

    GameObjectTable& gameobjects = std::get<GameObjectTable>(db.mTables);
    for(size_t i = 0; i < TableOperations::size(gameobjects); ++i) {
      std::get<FloatRow<Rot, Angle>>(gameobjects.mRows).at(i) += 0.01f * (i % 2 ? 1.0f : -1.f);
      static float hack = 0;
      hack += 0.001f;
      std::get<FloatRow<Pos, X>>(gameobjects.mRows).at(i) += std::sin(hack)*0.01f;
    }
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