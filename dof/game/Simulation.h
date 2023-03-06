#pragma once

#include "ConstraintsTableBuilder.h"
#include "Database.h"
#include "Table.h"

#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "SweepNPruneBroadphase.h"
#include "StableElementID.h"
#include "Scheduler.h"
#include "PhysicsTableIds.h"
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

  //The goal coordinates in X and Y that the given fragment wants to go to which will cause it to change to a static gameobject
  struct FragmentGoal{};

  struct X{};
  struct Y{};
  struct Angle{};
  struct CosAngle{};
  struct SinAngle{};
};

//1 if found, otherwise 0. Bitset or better would be nice but interop with ispc would be difficult
struct FragmentGoalFoundRow : Row<uint8_t> {};

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

struct SceneArgs {
  size_t mFragmentRows{};
  size_t mFragmentColumns{};
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

struct FileSystem {
  std::string mRoot;
};

using TextureRequestTable = Table<
  Row<TextureLoadRequest>
>;

using GlobalGameData = Table<
  SharedRow<SceneState>,
  SharedRow<PhysicsTableIds>,
  SharedRow<FileSystem>,
  SharedRow<StableElementMappings>,
  SharedRow<ConstraintsTableMappings>,
  SharedRow<Scheduler>
>;

using GameObjectTable = Table<
  FloatRow<Tags::Pos, Tags::X>,
  FloatRow<Tags::Pos, Tags::Y>,
  FloatRow<Tags::Rot, Tags::CosAngle>,
  FloatRow<Tags::Rot, Tags::SinAngle>,
  FloatRow<Tags::LinVel, Tags::X>,
  FloatRow<Tags::LinVel, Tags::Y>,
  FloatRow<Tags::AngVel, Tags::Angle>,
  FloatRow<Tags::FragmentGoal, Tags::X>,
  FloatRow<Tags::FragmentGoal, Tags::Y>,

  SweepNPruneBroadphase::OldMinX,
  SweepNPruneBroadphase::OldMinY,
  SweepNPruneBroadphase::OldMaxX,
  SweepNPruneBroadphase::OldMaxY,
  SweepNPruneBroadphase::NewMinX,
  SweepNPruneBroadphase::NewMinY,
  SweepNPruneBroadphase::NewMaxX,
  SweepNPruneBroadphase::NewMaxY,
  SweepNPruneBroadphase::NeedsReinsert,

  Row<CubeSprite>,
  FragmentGoalFoundRow,
  SharedRow<TextureReference>,

  StableIDRow
>;

using StaticGameObjectTable = Table<
  FloatRow<Tags::Pos, Tags::X>,
  FloatRow<Tags::Pos, Tags::Y>,
  FloatRow<Tags::Rot, Tags::CosAngle>,
  FloatRow<Tags::Rot, Tags::SinAngle>,
  //Only requires broadphase key to know how to remove it, don't need to store boundaries
  //for efficient updates because it won't move
  Row<CubeSprite>,
  SharedRow<TextureReference>,

  StableIDRow
>;

//Final desired move input state
struct PlayerInput {
  float mMoveX{};
  float mMoveY{};
  bool mAction1{};
  bool mAction2{};
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

  SweepNPruneBroadphase::OldMinX,
  SweepNPruneBroadphase::OldMinY,
  SweepNPruneBroadphase::OldMaxX,
  SweepNPruneBroadphase::OldMaxY,
  SweepNPruneBroadphase::NewMinX,
  SweepNPruneBroadphase::NewMinY,
  SweepNPruneBroadphase::NewMaxX,
  SweepNPruneBroadphase::NewMaxY,
  SweepNPruneBroadphase::NeedsReinsert,

  Row<CubeSprite>,
  Row<PlayerInput>,
  Row<PlayerKeyboardInput>,
  SharedRow<TextureReference>,

  StableIDRow
>;

struct Camera {
  float x{};
  float y{};
  float angle{};
  float zoom{};
};

struct DebugCameraControl {
  float mAdjustZoom{};
  bool mTakeSnapshot{};
  bool mLoadSnapshot{};
};

using CameraTable = Table<
  Row<Camera>,
  Row<DebugCameraControl>
>;

struct DebugPoint {
  glm::vec2 mPos;
  glm::vec3 mColor;
};

using DebugLineTable = Table<Row<DebugPoint>>;

using BroadphaseTable = SweepNPruneBroadphase::BroadphaseTable;

struct ForceData {
  struct Lifetime : Row<size_t> {};
  struct Strength : Row<float> {};
};

using GlobalPointForceTable = Table<
  FloatRow<Tags::Pos, Tags::X>,
  FloatRow<Tags::Pos, Tags::Y>,
  ForceData::Lifetime,
  ForceData::Strength
>;

using GameDatabase = Database<
  GlobalGameData,
  GameObjectTable,
  StaticGameObjectTable,
  TextureRequestTable,
  PlayerTable,
  CameraTable,
  BroadphaseTable,
  CollisionPairsTable,
  ConstraintsTable,
  ConstraintCommonTable,
  ContactConstraintsToStaticObjectsTable,
  DebugLineTable,
  GlobalPointForceTable
>;

struct Simulation {
  static void loadFromSnapshot(GameDatabase& db, const char* snapshotFilename);
  //Weird special case since graphics static is the one part that's not in the database
  static void snapshotInitGraphics(GameDatabase& db);
  static void writeSnapshot(GameDatabase& db, const char* snapshotFilename);

  static void update(GameDatabase& db);

  static SceneState::State _setupScene(GameDatabase& db, const SceneArgs& args);
  static TaskRange _updatePhysics(GameDatabase& db, const PhysicsConfig& config);
  //Insert all objects into the broadphase that can be. Doesn't check if they already are, meant for initial insert
  static void _initialPopulateBroadphase(GameDatabase& db);

  static void _checkFragmentGoals(GameDatabase& db);
  static void _migrateCompletedFragments(GameDatabase& db);
  static void _checkFragmentGoals(GameObjectTable& fragments);
  static void _migrateCompletedFragments(GameObjectTable& fragments, StaticGameObjectTable& destinationFragments, BroadphaseTable& broadphase, StableElementMappings& mappings);

  static PhysicsTableIds _getPhysicsTableIds();
  static Scheduler& _getScheduler(GameDatabase& db);
};