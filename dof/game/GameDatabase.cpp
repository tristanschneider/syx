#include "Precompile.h"
#include "GameDatabase.h"

#include "RuntimeDatabase.h"

#include "Simulation.h"
#include "stat/AllStatEffects.h"

#include "Fragment.h"
#include "Physics.h"
#include "SweepNPruneBroadphase.h"
#include "FragmentStateMachine.h"
#include "ConstraintSolver.h"
#include "Narrowphase.h"
#include "SpatialPairsStorage.h"
#include "SpatialQueries.h"
#include "GameInput.h"
#include "SceneNavigator.h"
#include "scenes/ImportedScene.h"
#include "scenes/LoadingScene.h"
#include "scenes/SceneList.h"
#include "shapes/AABB.h"
#include "shapes/Circle.h"
#include "shapes/Rectangle.h"
#include "shapes/Line.h"
#include "Constraints.h"
#include "loader/AssetService.h"

namespace GameDatabase {
  using BroadphaseTable = SweepNPruneBroadphase::BroadphaseTable;

  //Table to hold positions to be referenced by stable element id
  using TargetPosTable = Table<
    Tags::TableNameRow,
    SceneNavigator::IsClearedWithSceneTag,
    TargetTableTag,
    FloatRow<Tags::Pos, Tags::X>,
    FloatRow<Tags::Pos, Tags::Y>,
    StableIDRow
  >;

  using TextureRequestTable = Table<
    Tags::TableNameRow,
    Row<TextureLoadRequest>
  >;

  using GlobalGameData = Table<
    Tags::TableNameRow,
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
    Tags::TableNameRow,
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

  using DynamicPhysicsObjectsWithMotor = Table<
    Tags::TableNameRow,
    Tags::DynamicPhysicsObjectsWithMotorTag,
    SceneNavigator::IsClearedWithSceneTag,
    //Data viewed by physics, not to be used by gameplay
    FloatRow<Tags::Pos, Tags::X>,
    FloatRow<Tags::Pos, Tags::Y>,
    FloatRow<Tags::Rot, Tags::CosAngle>,
    FloatRow<Tags::Rot, Tags::SinAngle>,
    FloatRow<Tags::LinVel, Tags::X>,
    FloatRow<Tags::LinVel, Tags::Y>,
    FloatRow<Tags::LinVel, Tags::Z>,
    FloatRow<Tags::AngVel, Tags::Angle>,
    Tags::ScaleXRow,
    Tags::ScaleYRow,

    SweepNPruneBroadphase::BroadphaseKeys,
    Narrowphase::CollisionMaskRow,
    Narrowphase::SharedThicknessRow,
    Shapes::SharedRectangleRow,
    ConstraintSolver::ConstraintMaskRow,
    ConstraintSolver::MassRow,
    ConstraintSolver::SharedMaterialRow,

    Constraints::AutoManageJointTag,
    Constraints::TableConstraintDefinitionsRow,
    Constraints::ConstraintChangesRow,
    Constraints::JointRow,
    Constraints::ConstraintStorageRow,

    Row<CubeSprite>,
    SharedRow<TextureReference>,

    StableIDRow
  >;

  using DynamicPhysicsObjectsWithZ = Table<
    Tags::TableNameRow,
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
    Tags::TableNameRow,
    SceneNavigator::IsClearedWithSceneTag,
    FragmentSeekingGoalTagRow,
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

    Constraints::AutoManageJointTag,
    Constraints::TableConstraintDefinitionsRow,
    Constraints::ConstraintChangesRow,
    Constraints::CustomConstraintRow,
    Constraints::JointRow,
    Constraints::ConstraintStorageRow,

    FloatRow<Tags::FragmentGoal, Tags::X>,
    FloatRow<Tags::FragmentGoal, Tags::Y>,
    Fragment::FragmentGoalCooldownDefinitionRow,
    Fragment::FragmentGoalCooldownRow,
    FragmentGoalFoundRow,

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
    SharedRow<TextureReference>,
    IsFragment,

    StableIDRow
  >;

  using StaticGameObjectTable = Table<
    Tags::TableNameRow,
    SceneNavigator::IsClearedWithSceneTag,
    FragmentBurstStatEffect::CanTriggerFragmentBurstRow,
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
    Tags::TableNameRow,
    SceneNavigator::IsClearedWithSceneTag,
    FragmentBurstStatEffect::CanTriggerFragmentBurstRow,
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
    Tags::TableNameRow,
    SceneNavigator::IsClearedWithSceneTag,
    IsPlayer,
    Tags::ElementNeedsInitRow,

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

    Constraints::AutoManageJointTag,
    Constraints::TableConstraintDefinitionsRow,
    Constraints::ConstraintChangesRow,
    Constraints::JointRow,
    Constraints::ConstraintStorageRow,

    Row<CubeSprite>,
    GameInput::PlayerInputRow,
    GameInput::StateMachineRow,
    SharedRow<TextureReference>,
    StableIDRow
  >;
  using CameraTable = Table<
    Tags::TableNameRow,
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
    DynamicPhysicsObjectsWithZ,
    DynamicPhysicsObjectsWithMotor
  >;

  std::unique_ptr<IDatabase> create(StableElementMappings& mappings) {
    return DBReflect::mergeAll(
      DBReflect::createDatabase<GameDatabase>(mappings),
      DBReflect::createDatabase<StatEffectDatabase>(mappings),
      SceneNavigator::createDB(mappings),
      Scenes::createImportedSceneDB(mappings),
      Scenes::createLoadingSceneDB(mappings),
      Loader::createDB(mappings)
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

  //Set the name of the table that matches the filter
  template<class... Filter>
  void setName(IAppBuilder& builder, Tags::TableName name) {
    auto task = builder.createTask();
    task.setName("set table names");
    auto q = task.query<Tags::TableNameRow, const Filter...>();
    assert(q.size() <= 1);
    if(!q.size()) {
      task.discard();
      return;
    }
    task.setCallback([q, n{std::move(name)}](AppTaskArgs&) mutable {
      q.get<0>(0).at() = std::move(n);
    });
    builder.submitTask(std::move(task));
  }

  template<class... Filter>
  void configureSelfMotor(IAppBuilder& builder) {
    auto task = builder.createTask();
    auto q = task.query<Constraints::TableConstraintDefinitionsRow, const Filter...>();
    auto res = task.getResolver<const Constraints::CustomConstraintRow>();
    task.setCallback([q, res](AppTaskArgs&) mutable {
      for(size_t t = 0; t < q.size(); ++t) {
        auto& definitions = q.get<0>(t);
        Constraints::Definition def;
        if(res->tryGetRow<const Constraints::CustomConstraintRow>(q.matchingTableIDs[t])) {
          def.custom = def.custom.create();
        }
        def.targetA = Constraints::SelfTarget{};
        def.targetB = Constraints::NoTarget{};
        def.joint = def.joint.create();
        def.storage = def.storage.create();
        definitions.at().definitions.push_back(def);
      }
    });
    builder.submitTask(std::move(task.setName("init player motor")));
  }

  void configureDefaults(IAppBuilder& builder) {
    setName<TargetTableTag>(builder, { "Targets" });
    setName<Row<TextureLoadRequest>>(builder, { "Texture Requests" });
    setName<ShapeRegistry::GlobalRow>(builder, { "Globals" });
    setName<Tags::DynamicPhysicsObjectsTag>(builder, { "Physics Objects" });
    setName<Tags::DynamicPhysicsObjectsWithZTag>(builder, { "Physics Objects Z" });
    setName<SharedMassObjectTableTag>(builder, { "Active Fragments" });
    setName<ZeroMassObjectTableTag, FragmentGoalFoundTableTag>(builder, { "Completed Fragments" });
    setName<Tags::TerrainRow>(builder, { "Terrain" });
    setName<IsPlayer>(builder, { "Players" });
    setName<Row<Camera>>(builder, { "Cameras" });
    setName<Tags::DynamicPhysicsObjectsWithMotorTag>(builder, { "Dynamic With Motor" });

    configureSelfMotor<Tags::DynamicPhysicsObjectsWithMotorTag>(builder);
    configureSelfMotor<FragmentSeekingGoalTagRow>(builder);

    const auto defaultQuadMass = Geo::computeQuadMass(1.0f, 1.0f, 1.0f);
    setDefaultValue<ConstraintSolver::MassRow, Shapes::SharedRectangleRow>(builder, "rect", defaultQuadMass);
    setDefaultValue<Tags::ScaleXRow>(builder, "sx", 1.0f);
    setDefaultValue<Tags::ScaleYRow>(builder, "sy", 1.0f);
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
    setDefaultValue<Tags::ElementNeedsInitRow>(builder, "setDefault needsInit", (uint8_t)1);
  }

  template<class... Filter>
  TableID getOrAssertTable(RuntimeDatabaseTaskBuilder& task) {
    auto q = task.queryTables<Filter...>();
    assert(q.size());
    return q[0];
  }

  Tables::Tables(RuntimeDatabaseTaskBuilder& task)
    : player{ getOrAssertTable<IsPlayer>(task) }
    , terrain{ getOrAssertTable<Tags::TerrainRow>(task) }
    , activeFragment{ getOrAssertTable<FragmentSeekingGoalTagRow>(task) }
    , completedFragment{ getOrAssertTable<FragmentGoalFoundTableTag>(task) }
  {
  }
}
