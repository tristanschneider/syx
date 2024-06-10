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
#include "GameInput.h"
#include "SceneNavigator.h"
#include "scenes/SceneList.h"
#include "shapes/AABB.h"
#include "shapes/Circle.h"
#include "shapes/Rectangle.h"
#include "shapes/Line.h"

namespace GameDatabase {
  using BroadphaseTable = SweepNPruneBroadphase::BroadphaseTable;

  //Table to hold positions to be referenced by stable element id
  using TargetPosTable = Table<
    SceneNavigator::IsClearedWithSceneTag,
    TargetTableTag,
    FloatRow<Tags::Pos, Tags::X>,
    FloatRow<Tags::Pos, Tags::Y>,
    StableIDRow
  >;

  using TextureRequestTable = Table<
    Row<TextureLoadRequest>
  >;

  using GlobalGameData = Table<
    ShapeRegistry::GlobalRow,
    SceneList::ScenesRow,
    SharedRow<SceneState>,
    SharedRow<FileSystem>,
    SharedRow<StableElementMappings>,
    SharedRow<Scheduler>,
    SharedRow<Config::GameConfig>,
    GameInput::GlobalMappingsRow,
    Events::EventsRow,

    ThreadLocalsRow
  >;

  using DynamicPhysicsObjects = Table<
    Tags::DynamicPhysicsObjectsTag,
    SceneNavigator::IsClearedWithSceneTag,
    //Data viewed by physics, not to be used by gameplay
    FloatRow<Tags::Pos, Tags::X>,
    FloatRow<Tags::Pos, Tags::Y>,
    FloatRow<Tags::Rot, Tags::CosAngle>,
    FloatRow<Tags::Rot, Tags::SinAngle>,
    FloatRow<Tags::LinVel, Tags::X>,
    FloatRow<Tags::LinVel, Tags::Y>,
    FloatRow<Tags::AngVel, Tags::Angle>,
    Tags::ScaleXRow,
    Tags::ScaleYRow,

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

    AccelerationY,

    SweepNPruneBroadphase::BroadphaseKeys,
    Narrowphase::CollisionMaskRow,
    Narrowphase::SharedThicknessRow,
    Shapes::SharedRectangleRow,
    ConstraintSolver::ConstraintMaskRow,
    ConstraintSolver::MassRow,
    ConstraintSolver::SharedMaterialRow,

    Row<CubeSprite>,
    SharedRow<TextureReference>,

    StableIDRow
  >;

  using DynamicPhysicsObjectsWithZ = Table<
    Tags::DynamicPhysicsObjectsWithZTag,
    SceneNavigator::IsClearedWithSceneTag,
    //Data viewed by physics, not to be used by gameplay
    FloatRow<Tags::Pos, Tags::X>,
    FloatRow<Tags::Pos, Tags::Y>,
    FloatRow<Tags::Pos, Tags::Z>,
    FloatRow<Tags::Rot, Tags::CosAngle>,
    FloatRow<Tags::Rot, Tags::SinAngle>,
    FloatRow<Tags::LinVel, Tags::X>,
    FloatRow<Tags::LinVel, Tags::Y>,
    FloatRow<Tags::LinVel, Tags::Z>,
    FloatRow<Tags::AngVel, Tags::Angle>,
    Tags::ScaleXRow,
    Tags::ScaleYRow,

    //Gameplay data extracted from above
    FloatRow<Tags::GPos, Tags::X>,
    FloatRow<Tags::GPos, Tags::Y>,
    FloatRow<Tags::GPos, Tags::Z>,
    FloatRow<Tags::GRot, Tags::CosAngle>,
    FloatRow<Tags::GRot, Tags::SinAngle>,
    FloatRow<Tags::GLinVel, Tags::X>,
    FloatRow<Tags::GLinVel, Tags::Y>,
    FloatRow<Tags::GLinVel, Tags::Z>,
    FloatRow<Tags::GAngVel, Tags::Angle>,

    //Impulses requested from gameplay
    FloatRow<Tags::GLinImpulse, Tags::X>,
    FloatRow<Tags::GLinImpulse, Tags::Y>,
    FloatRow<Tags::GLinImpulse, Tags::Z>,
    FloatRow<Tags::GAngImpulse, Tags::Angle>,

    SweepNPruneBroadphase::BroadphaseKeys,
    Narrowphase::CollisionMaskRow,
    Narrowphase::SharedThicknessRow,
    Shapes::SharedRectangleRow,
    ConstraintSolver::ConstraintMaskRow,
    ConstraintSolver::MassRow,
    ConstraintSolver::SharedMaterialRow,

    Row<CubeSprite>,
    SharedRow<TextureReference>,

    StableIDRow
  >;

  using GameObjectTable = Table<
    SceneNavigator::IsClearedWithSceneTag,
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
    Narrowphase::SharedThicknessRow,
    Shapes::SharedRectangleRow,
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
    SceneNavigator::IsClearedWithSceneTag,
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
    Narrowphase::SharedThicknessRow,
    Shapes::SharedRectangleRow,
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

  using TerrainTable = Table<
    SceneNavigator::IsClearedWithSceneTag,
    ZeroMassObjectTableTag,
    Tags::TerrainRow,
    Shapes::SharedRectangleRow,
    FloatRow<Tags::Pos, Tags::X>,
    FloatRow<Tags::Pos, Tags::Y>,
    FloatRow<Tags::Pos, Tags::Z>,
    FloatRow<Tags::Rot, Tags::CosAngle>,
    FloatRow<Tags::Rot, Tags::SinAngle>,
    Tags::ScaleXRow,
    Tags::ScaleYRow,

    //Gameplay data extracted from above
    //TODO: take advantage of immobility to avoid the need for this
    FloatRow<Tags::GPos, Tags::X>,
    FloatRow<Tags::GPos, Tags::Y>,
    FloatRow<Tags::GPos, Tags::Z>,
    FloatRow<Tags::GRot, Tags::CosAngle>,
    FloatRow<Tags::GRot, Tags::SinAngle>,

    SweepNPruneBroadphase::BroadphaseKeys,
    Narrowphase::CollisionMaskRow,
    Narrowphase::SharedThicknessRow,
    ConstraintSolver::ConstraintMaskRow,
    ConstraintSolver::SharedMassRow,
    ConstraintSolver::SharedMaterialRow,

    Row<CubeSprite>,
    SharedRow<TextureReference>,
    IsImmobile,

    StableIDRow
  >;

  using PlayerTable = Table<
    SceneNavigator::IsClearedWithSceneTag,
    IsPlayer,
    FloatRow<Tags::Pos, Tags::X>,
    FloatRow<Tags::Pos, Tags::Y>,
    FloatRow<Tags::Pos, Tags::Z>,
    FloatRow<Tags::Rot, Tags::CosAngle>,
    FloatRow<Tags::Rot, Tags::SinAngle>,
    FloatRow<Tags::LinVel, Tags::X>,
    FloatRow<Tags::LinVel, Tags::Y>,
    FloatRow<Tags::LinVel, Tags::Z>,
    FloatRow<Tags::AngVel, Tags::Angle>,
    AccelerationZ,

    //Gameplay data extracted from above
    FloatRow<Tags::GPos, Tags::X>,
    FloatRow<Tags::GPos, Tags::Y>,
    FloatRow<Tags::GPos, Tags::Z>,
    FloatRow<Tags::GRot, Tags::CosAngle>,
    FloatRow<Tags::GRot, Tags::SinAngle>,
    FloatRow<Tags::GLinVel, Tags::X>,
    FloatRow<Tags::GLinVel, Tags::Y>,
    FloatRow<Tags::GAngVel, Tags::Angle>,

    //Impulses requested from gameplay
    FloatRow<Tags::GLinImpulse, Tags::X>,
    FloatRow<Tags::GLinImpulse, Tags::Y>,
    FloatRow<Tags::GLinImpulse, Tags::Z>,
    FloatRow<Tags::GAngImpulse, Tags::Angle>,

    SweepNPruneBroadphase::BroadphaseKeys,
    Narrowphase::CollisionMaskRow,
    Narrowphase::SharedThicknessRow,
    Shapes::SharedRectangleRow,
    ConstraintSolver::ConstraintMaskRow,
    ConstraintSolver::SharedMassRow,
    ConstraintSolver::SharedMaterialRow,

    Row<CubeSprite>,
    GameInput::PlayerInputRow,
    GameInput::StateMachineRow,
    SharedRow<TextureReference>,
    StableIDRow
  >;
  using CameraTable = Table<
    Row<Camera>,
    FloatRow<Tags::Pos, Tags::X>,
    FloatRow<Tags::Pos, Tags::Y>,
    GameInput::StateMachineRow,
    GameInput::CameraDebugInputRow,
    StableIDRow
  >;

  using GameDatabase = Database<
    SpatialQuery::AABBSpatialQueriesTable,
    SpatialQuery::CircleSpatialQueriesTable,
    SpatialQuery::RaycastSpatialQueriesTable,
    BroadphaseTable,
    SP::SpatialPairsTable,
    GlobalGameData,
    TerrainTable,
    GameObjectTable,
    StaticGameObjectTable,
    TextureRequestTable,
    PlayerTable,
    CameraTable,
    DebugLineTable,
    DebugTextTable,
    TargetPosTable,
    DynamicPhysicsObjects,
    DynamicPhysicsObjectsWithZ
  >;

  std::unique_ptr<IDatabase> create(StableElementMappings& mappings) {
    return DBReflect::mergeAll(
      DBReflect::createDatabase<GameDatabase>(mappings),
      DBReflect::createDatabase<StatEffectDatabase>(mappings),
      SceneNavigator::createDB(mappings)
    );
  }

  //Does a query and sets the default value of the first query element
  template<class... Query, class T>
  void setDefaultValue(IAppBuilder& builder, std::string_view taskName, T value) {
    auto task = builder.createTask();
    task.setName(taskName);
    auto q = task.query<Query...>();
    task.setCallback([q, value](AppTaskArgs&) mutable {
      q.forEachRow([value](auto& row, auto&...) { row.setDefaultValue(value); });
    });
    builder.submitTask(std::move(task));
  }

  void configureDefaults(IAppBuilder& builder) {
    setDefaultValue<FloatRow<Tags::Rot, Tags::CosAngle>>(builder, "setDefault Rot", 1.0f);
    setDefaultValue<FloatRow<Tags::GRot, Tags::CosAngle>>(builder, "setDefault GRot", 1.0f);
    setDefaultValue<Narrowphase::CollisionMaskRow>(builder, "setDefault Mask", uint8_t(~0));
    setDefaultValue<ConstraintSolver::ConstraintMaskRow>(builder, "setDefault Constraint Mask", ConstraintSolver::MASK_SOLVE_ALL);
    setDefaultValue<ConstraintSolver::SharedMassRow, Shapes::SharedRectangleRow>(builder, "setDefault mass", Geo::computeQuadMass(1, 1, 1));
    setDefaultValue<ConstraintSolver::SharedMassRow, ZeroMassObjectTableTag>(builder, "set zero mass", Geo::BodyMass{});
    //Fragments in particular start opaque then reveal the texture as they take damage
    setDefaultValue<Tint, const IsFragment>(builder, "setDefault Tint", glm::vec4(0, 0, 0, 1));
    setDefaultValue<AccelerationZ>(builder, "set acceleration", -0.01f);
    setDefaultValue<Narrowphase::SharedThicknessRow>(builder, "thickness", 0.1f);
    setDefaultValue<Narrowphase::SharedThicknessRow, const Tags::TerrainRow>(builder, "terrainthickness", 0.0f);
  }
}
