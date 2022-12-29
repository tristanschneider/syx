#pragma once

#include "Database.h"
#include "Physics.h"
#include "Table.h"

#include "glm/vec2.hpp"

#include <bitset>

template<class, class = void>
struct FloatRow : Row<float> {};

namespace Tags {
  //Position in X and Y
  struct Pos{};
  //Rotation is stored as cos(angle) sin(angle), which is the first column of the rotation matrix and can be used to construct the full
  //rotation matrix since it's symmetric
  //They must be initialized to valid values, like cos 1 sin 0
  struct Rot{};
  //Linear velocity in X and Y
  struct LinVel{};
  //Angular velocity in Angle
  struct AngVel{};

  struct X{};
  struct Y{};
  struct Angle{};
  struct CosAngle{};
  struct SinAngle{};
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
  size_t mPlayerImage = 0;
  glm::vec2 mBoundaryMin, mBoundaryMax;
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
  FloatRow<Tags::Rot, Tags::CosAngle>,
  FloatRow<Tags::Rot, Tags::SinAngle>,
  FloatRow<Tags::LinVel, Tags::X>,
  FloatRow<Tags::LinVel, Tags::Y>,
  FloatRow<Tags::AngVel, Tags::Angle>,
  Row<CubeSprite>,
  SharedRow<TextureReference>
>;

//Final desired move input state
struct PlayerInput {
  float mMoveX{};
  float mMoveY{};
};

//Intermediate keyboard state used to compute final state
struct PlayerKeyboardInput {
  enum class Key : uint8_t {
    Up,
    Down,
    Left,
    Right,
    Count,
  };
  std::bitset<(size_t)Key::Count> mKeys;
};

using PlayerTable = Table<
  FloatRow<Tags::Pos, Tags::X>,
  FloatRow<Tags::Pos, Tags::Y>,
  FloatRow<Tags::Rot, Tags::CosAngle>,
  FloatRow<Tags::Rot, Tags::SinAngle>,
  FloatRow<Tags::LinVel, Tags::X>,
  FloatRow<Tags::LinVel, Tags::Y>,
  FloatRow<Tags::AngVel, Tags::Angle>,
  Row<CubeSprite>,
  Row<PlayerInput>,
  Row<PlayerKeyboardInput>,
  SharedRow<TextureReference>
>;

struct Camera {
  float x{};
  float y{};
  float angle{};
  float zoom{};
};

struct DebugCameraControl {
  float mAdjustZoom{};
};

using CameraTable = Table<
  Row<Camera>,
  Row<DebugCameraControl>
>;

using BroadphaseTable = GridBroadphase::BroadphaseTable;

using GameDatabase = Database<
  GameObjectTable,
  GlobalGameData,
  TextureRequestTable,
  PlayerTable,
  CameraTable,
  BroadphaseTable,
  CollisionPairsTable,
  ConstraintsTable
>;

struct Simulation {
  static void update(GameDatabase& db);
};