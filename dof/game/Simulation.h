#pragma once

#include "ConstraintsTableBuilder.h"
#include "Database.h"
#include "Table.h"

#include "Config.h"
#include "FragmentStateMachine.h"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"
#include "SpatialQueries.h"
#include "SweepNPruneBroadphase.h"
#include "StableElementID.h"
#include "Scheduler.h"
#include "PhysicsTableIds.h"
#include <bitset>
#include "DBEvents.h"
#include "RowTags.h"

namespace Ability {
  struct AbilityInput;
}

class IAppBuilder;

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
  glm::vec2 mBoundaryMin{};
  glm::vec2 mBoundaryMax{};
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
  SharedRow<Config::GameConfig>,
  Events::EventsRow,

  ThreadLocalsRow,
  ExternalDatabasesRow
>;

struct IsImmobile : TagRow{};
struct IsFragment : TagRow{};
struct DamageTaken : Row<float>{};
struct Tint : Row<glm::vec4>{};
enum class FragmentFlags : uint8_t {
  InBounds = 1 << 0,
};
struct FragmentFlagsRow : Row<FragmentFlags>{};

using GameObjectTable = Table<
  SharedMassObjectTableTag,
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
  DamageTaken,
  Tint,
  FragmentFlagsRow,
  FragmentStateMachine::StateRow,

  CollisionMaskRow,
  SweepNPruneBroadphase::BroadphaseKeys,

  Row<CubeSprite>,
  FragmentGoalFoundRow,
  SharedRow<TextureReference>,
  IsFragment,

  StableIDRow
>;

struct FragmentGoalFoundTableTag : TagRow {};

using StaticGameObjectTable = Table<
  ZeroMassObjectTableTag,
  FragmentGoalFoundTableTag,
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

  CollisionMaskRow,
  SweepNPruneBroadphase::BroadphaseKeys,

  //Only requires broadphase key to know how to remove it, don't need to store boundaries
  //for efficient updates because it won't move
  Row<CubeSprite>,
  SharedRow<TextureReference>,
  IsImmobile,
  IsFragment,

  StableIDRow
>;

//Table to hold positions to be referenced by stable element id
using TargetPosTable = Table<
  FloatRow<Tags::Pos, Tags::X>,
  FloatRow<Tags::Pos, Tags::Y>,
  StableIDRow
>;

enum class KeyState : uint8_t {
  Up,
  Triggered,
  Released,
  Down,
};

//Final desired move input state
struct PlayerInput {
  PlayerInput();
  PlayerInput(PlayerInput&&);
  ~PlayerInput();

  PlayerInput& operator=(PlayerInput&&);

  float mMoveX{};
  float mMoveY{};
  KeyState mAction1{};
  KeyState mAction2{};
  //Goes from 0 to 1 when starting input in a direction, then back down to zero when stopping
  float moveT{};
  float angularMoveT{};

  std::unique_ptr<Ability::AbilityInput> ability1;
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

struct IsPlayer : SharedRow<char> {};

using PlayerTable = Table<
  IsPlayer,
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

  CollisionMaskRow,
  SweepNPruneBroadphase::BroadphaseKeys,

  Row<CubeSprite>,
  Row<PlayerInput>,
  Row<PlayerKeyboardInput>,
  SharedRow<TextureReference>,

  StableIDRow
>;

struct Camera {
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
  FloatRow<Tags::Pos, Tags::X>,
  FloatRow<Tags::Pos, Tags::Y>,
  Row<DebugCameraControl>,
  StableIDRow
>;

struct DebugPoint {
  glm::vec2 mPos;
  glm::vec3 mColor;
};

using DebugLineTable = Table<Row<DebugPoint>>;

using BroadphaseTable = SweepNPruneBroadphase::BroadphaseTable;

using GameDatabase = Database<
  SpatialQuery::SpatialQueriesTable,
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
  TargetPosTable
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

namespace Simulation {
  void init(GameDatabase& db);

  void buildUpdateTasks(IAppBuilder& builder);

  const SceneState& _getSceneState(GameDatabase& db);
  Scheduler& _getScheduler(GameDatabase& db);

  const char* getConfigName();
};