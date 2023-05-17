#pragma once

#include "ConstraintsTableBuilder.h"
#include "Database.h"
#include "Table.h"

#include "Config.h"
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

  struct GPos{};
  struct GRot{};
  struct GLinVel{};
  struct GAngVel{};

  //Impulses from gameplay to apply. In other words a desired change to LinVel
  //Equivalent to making a velocity stat effect targeting this elemtn of lifetime 1
  struct GLinImpulse{};
  struct GAngImpulse{};

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

struct ThreadLocals;
struct ThreadLocalsInstance {
  ThreadLocalsInstance();
  ~ThreadLocalsInstance();

  std::unique_ptr<ThreadLocals> instance;
};

struct ThreadLocalsRow : SharedRow<ThreadLocalsInstance> {};

using TextureRequestTable = Table<
  Row<TextureLoadRequest>
>;

struct StatEffectDBOwned;

struct ExternalDatabases {
  ExternalDatabases();
  ~ExternalDatabases();

  std::unique_ptr<StatEffectDBOwned> statEffects;
};
struct ExternalDatabasesRow : SharedRow<ExternalDatabases> {};

using GlobalGameData = Table<
  SharedRow<SceneState>,
  SharedRow<PhysicsTableIds>,
  SharedRow<FileSystem>,
  SharedRow<StableElementMappings>,
  SharedRow<ConstraintsTableMappings>,
  SharedRow<Scheduler>,
  ThreadLocalsRow,
  ExternalDatabasesRow
>;

struct IsImmobile : SharedRow<char>{};

using GameObjectTable = Table<
  //Data viewed by physics, not to be used by gameplay
  FloatRow<Tags::Pos, Tags::X>,
  FloatRow<Tags::Pos, Tags::Y>,
  FloatRow<Tags::Rot, Tags::CosAngle>,
  FloatRow<Tags::Rot, Tags::SinAngle>,
  FloatRow<Tags::LinVel, Tags::X>,
  FloatRow<Tags::LinVel, Tags::Y>,
  FloatRow<Tags::AngVel, Tags::Angle>,

  //Gameplay data extracted from above
  FloatRow<Tags::GPos, Tags::X>,
  FloatRow<Tags::GPos, Tags::Y>,
  FloatRow<Tags::GRot, Tags::CosAngle>,
  FloatRow<Tags::GRot, Tags::SinAngle>,
  FloatRow<Tags::GLinVel, Tags::X>,
  FloatRow<Tags::GLinVel, Tags::Y>,
  FloatRow<Tags::GAngVel, Tags::Angle>,

  //Impulses requested from gameplay
  FloatRow<Tags::GLinImpulse, Tags::X>,
  FloatRow<Tags::GLinImpulse, Tags::Y>,
  FloatRow<Tags::GAngImpulse, Tags::Angle>,

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

  //Gameplay data extracted from above
  //TODO: take advantage of immobility to avoid the need for this
  FloatRow<Tags::GPos, Tags::X>,
  FloatRow<Tags::GPos, Tags::Y>,
  FloatRow<Tags::GRot, Tags::CosAngle>,
  FloatRow<Tags::GRot, Tags::SinAngle>,

  //Only requires broadphase key to know how to remove it, don't need to store boundaries
  //for efficient updates because it won't move
  Row<CubeSprite>,
  SharedRow<TextureReference>,
  IsImmobile,

  StableIDRow
>;

//Final desired move input state
struct PlayerInput {
  float mMoveX{};
  float mMoveY{};
  bool mAction1{};
  bool mAction2{};
};

enum class KeyState : uint8_t {
  Triggered,
  Released
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
  glm::vec2 mRawMousePixels{};
  glm::vec2 mRawMouseDeltaPixels{};
  glm::vec2 mLastMousePos{};
  bool mIsRelativeMouse{};
  float mRawWheelDelta{};
  std::vector<std::pair<KeyState, int>> mRawKeys;
  std::string mRawText;
};

using PlayerTable = Table<
  FloatRow<Tags::Pos, Tags::X>,
  FloatRow<Tags::Pos, Tags::Y>,
  FloatRow<Tags::Rot, Tags::CosAngle>,
  FloatRow<Tags::Rot, Tags::SinAngle>,
  FloatRow<Tags::LinVel, Tags::X>,
  FloatRow<Tags::LinVel, Tags::Y>,
  FloatRow<Tags::AngVel, Tags::Angle>,

  //Gameplay data extracted from above
  FloatRow<Tags::GPos, Tags::X>,
  FloatRow<Tags::GPos, Tags::Y>,
  FloatRow<Tags::GRot, Tags::CosAngle>,
  FloatRow<Tags::GRot, Tags::SinAngle>,
  FloatRow<Tags::GLinVel, Tags::X>,
  FloatRow<Tags::GLinVel, Tags::Y>,
  FloatRow<Tags::GAngVel, Tags::Angle>,

  //Impulses requested from gameplay
  FloatRow<Tags::GLinImpulse, Tags::X>,
  FloatRow<Tags::GLinImpulse, Tags::Y>,
  FloatRow<Tags::GAngImpulse, Tags::Angle>,

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
  ConfigTable
>;

//For allowing forward declarations where GameDatabase is desired
struct GameDB { GameDatabase& db; };

struct SimulationPhases {
  //Root, no dependencies
  TaskRange root;
  //Process and delete render requets
  TaskRange renderRequests;
  //Graphics reads from sprites
  TaskRange renderExtraction;
  TaskRange simulation;
  //Do the rendering. Does not require access to GameDB
  TaskRange render;
  TaskRange imgui;
  TaskRange swapBuffers;
};

struct Simulation {
  static void loadFromSnapshot(GameDatabase& db, const char* snapshotFilename);
  //Weird special case since graphics static is the one part that's not in the database
  static void snapshotInitGraphics(GameDatabase& db);
  static void writeSnapshot(GameDatabase& db, const char* snapshotFilename);

  static void init(GameDatabase& db);

  static void buildUpdateTasks(GameDatabase& db, SimulationPhases& phases);
  static void linkUpdateTasks(SimulationPhases& phases);

  static const SceneState& _getSceneState(GameDatabase& db);
  static Scheduler& _getScheduler(GameDatabase& db);
};