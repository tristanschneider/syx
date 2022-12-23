#include "Precompile.h"
#include "Simulation.h"

#include "Queries.h"

namespace {
  using namespace Tags;
  SceneState::State _initRequestAssets(GameDatabase& db) {
    TextureRequestTable& textureRequests = std::get<TextureRequestTable>(db.mTables);
    TextureLoadRequest* request = &TableOperations::addToTable(textureRequests).get<0>();
    request->mFileName = "C:/Users/forgo/Downloads/Crash_Bandicoot_Cover.png";
    request->mImageID = std::hash<std::string>()(request->mFileName);
    std::get<0>(std::get<GlobalGameData>(db.mTables).mRows).at().mBackgroundImage = request->mImageID;

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

    //Make all the objects use the background image as their texture
    std::get<SharedRow<TextureReference>>(gameobjects.mRows).at().mId = std::get<0>(std::get<GlobalGameData>(db.mTables).mRows).at().mBackgroundImage;

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
    Queries::viewEachRow<FloatRow<Rot, Angle>>(db, [](FloatRow<Rot, Angle>& row) {
      for(size_t i = 0; i < row.mElements.size(); ++i) {
    
        row.mElements[i] += 0.01f * (i % 2 ? 1.0f : -1.f);
      }
    });
    Queries::viewEachRow<FloatRow<Pos, X>>(db, [](FloatRow<Pos, X>& row) {
      static float hack = 0;
      for(float& x : row.mElements) {
        hack += 0.001f;
    
        x += std::sin(hack)*0.01f;
      }
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