#include "Precompile.h"
#include "GameDatabase.h"

#include "RuntimeDatabase.h"

#include "Simulation.h"
#include "stat/AllStatEffects.h"

#include "Physics.h"
#include "SweepNPruneBroadphase.h"
#include "FragmentStateMachine.h"
#include "ConstraintSolver.h"
#include "Narrowphase.h"
#include "SpatialPairsStorage.h"
#include "SpatialQueries.h"

namespace GameData {
  using BroadphaseTable = SweepNPruneBroadphase::BroadphaseTable;

  //Table to hold positions to be referenced by stable element id
  using TargetPosTable = Table<
    TargetTableTag,
    FloatRow<Tags::Pos, Tags::X>,
    FloatRow<Tags::Pos, Tags::Y>,
    StableIDRow
  >;

  using TextureRequestTable = Table<
    Row<TextureLoadRequest>
  >;

  using GlobalGameData = Table<
    SharedRow<SceneState>,
    SharedRow<FileSystem>,
    SharedRow<StableElementMappings>,
    SharedRow<Scheduler>,
    SharedRow<Config::GameConfig>,
    Events::EventsRow,

    ThreadLocalsRow
  >;

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
    FragmentStateMachine::GlobalsRow,

    SweepNPruneBroadphase::BroadphaseKeys,
    Narrowphase::CollisionMaskRow,
    Narrowphase::SharedUnitCubeRow,
    ConstraintSolver::ConstraintMaskRow,
    ConstraintSolver::SharedMassRow,
    ConstraintSolver::SharedMaterialRow,

    Row<CubeSprite>,
    FragmentGoalFoundRow,
    SharedRow<TextureReference>,
    IsFragment,

    StableIDRow
  >;

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

    SweepNPruneBroadphase::BroadphaseKeys,
    Narrowphase::CollisionMaskRow,
    Narrowphase::SharedUnitCubeRow,
    ConstraintSolver::ConstraintMaskRow,
    ConstraintSolver::SharedMassRow,
    ConstraintSolver::SharedMaterialRow,

    //Only requires broadphase key to know how to remove it, don't need to store boundaries
    //for efficient updates because it won't move
    Row<CubeSprite>,
    SharedRow<TextureReference>,
    IsImmobile,
    IsFragment,

    StableIDRow
  >;

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

    SweepNPruneBroadphase::BroadphaseKeys,
    Narrowphase::CollisionMaskRow,
    Narrowphase::SharedUnitCubeRow,
    ConstraintSolver::ConstraintMaskRow,
    ConstraintSolver::SharedMassRow,
    ConstraintSolver::SharedMaterialRow,

    Row<CubeSprite>,
    Row<PlayerInput>,
    Row<PlayerKeyboardInput>,
    SharedRow<TextureReference>,

    StableIDRow
  >;
  using CameraTable = Table<
    Row<Camera>,
    FloatRow<Tags::Pos, Tags::X>,
    FloatRow<Tags::Pos, Tags::Y>,
    Row<DebugCameraControl>,
    StableIDRow
  >;

  using GameDatabase = Database<
    SpatialQuery::AABBSpatialQueriesTable,
    SpatialQuery::CircleSpatialQueriesTable,
    SpatialQuery::RaycastSpatialQueriesTable,
    BroadphaseTable,
    SP::SpatialPairsTable,
    GlobalGameData,
    GameObjectTable,
    StaticGameObjectTable,
    TextureRequestTable,
    PlayerTable,
    CameraTable,
    DebugLineTable,
    DebugTextTable,
    TargetPosTable
  >;


  std::unique_ptr<IDatabase> create(StableElementMappings& mappings) {
    return DBReflect::merge(
      DBReflect::createDatabase<GameDatabase>(mappings),
      DBReflect::createDatabase<StatEffectDatabase>(mappings)
    );
  }
}
