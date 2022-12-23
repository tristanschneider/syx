#pragma once

#include "Database.h"
#include "Table.h"

template<class, class = void>
struct FloatRow : Row<float> {};

namespace Tags {
  struct Pos{};
  struct Rot{};

  struct X{};
  struct Y{};
  struct Angle{};
};

enum class RequestStatus : uint8_t {
  InProgress,
  Failed,
  Succeeded
};

struct TextureLoadRequest {
  //This is the id assigned by the creator of the request which is used to refer to the image later
  size_t mImageID = 0;
  std::string mFileName;
  //Set by the handler of the request while processing it to communicate information back to the creator
  RequestStatus mStatus = RequestStatus::InProgress;
};

struct CubeSprite {
  float uMin = 0;
  float vMin = 0;
  float uMax = 1;
  float vMax = 1;
};

//Shared reference for all objects in the table to use
struct TextureReference {
  size_t mId = 0;
};

struct SceneState {
  enum class State : uint8_t {
    InitRequestAssets,
    InitAwaitingAssets,
    SetupScene,
    Update
  };
  State mState = State::InitRequestAssets;
  size_t mBackgroundImage = 0;
};

using TextureRequestTable = Table<
  Row<TextureLoadRequest>
>;

using GlobalGameData = Table<
  SharedRow<SceneState>
>;

using GameObjectTable = Table<
  FloatRow<Tags::Pos, Tags::X>,
  FloatRow<Tags::Pos, Tags::Y>,
  FloatRow<Tags::Rot, Tags::Angle>,
  Row<CubeSprite>,
  SharedRow<TextureReference>
>;

using GameDatabase = Database<
  GameObjectTable,
  GlobalGameData,
  TextureRequestTable
>;

struct Simulation {
  static void update(GameDatabase& db);
};