#include "GameDatabase.h"

#include "RuntimeDatabase.h"

#include "Simulation.h"
#include "stat/AllStatEffects.h"
#include "stat/FragmentBurstStatEffect.h"

#include "Fragment.h"
#include "Physics.h"
#include "SweepNPruneBroadphase.h"
#include "FragmentSpawner.h"
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
#include <shapes/Mesh.h>
#include <math/Constants.h>
#include "loader/AssetService.h"
#include "GraphicsTables.h"
#include "TableName.h"
#include <module/MassModule.h>
#include <module/PhysicsEvents.h>
#include <PhysicsTableBuilder.h>
#include <transform/TransformModule.h>
#include <math/AxisFlags.h>

namespace GameDatabase {
  using BroadphaseTable = SweepNPruneBroadphase::BroadphaseTable;

  StorageTableBuilder createTargetPosTable() {
    StorageTableBuilder table;
    Transform::addPosXY(table)
    .addRows<
      SceneNavigator::IsClearedWithSceneTag,
      TargetTableTag
    >()
    .setStable()
    .setTableName({ "Targets" });
    return table;
  }

  StorageTableBuilder createGlobalTable() {
    StorageTableBuilder table;
    table.addRows<
      ShapeRegistry::GlobalRow,
      SceneList::ScenesRow,
      SharedRow<SceneState>,
      SharedRow<FileSystem>,
      SharedRow<StableElementMappings>,
      SharedRow<Scheduler>,
      SharedRow<Config::GameConfig>,
      GameInput::GlobalMappingsRow,
      ThreadLocalsRow
    >().setTableName({ "Globals" });
    return table;
  }

  using GlobalGameData = Table<
    TableName::TableNameRow,
    ShapeRegistry::GlobalRow,
    SceneList::ScenesRow,
    SharedRow<SceneState>,
    SharedRow<FileSystem>,
    SharedRow<StableElementMappings>,
    SharedRow<Scheduler>,
    SharedRow<Config::GameConfig>,
    GameInput::GlobalMappingsRow,

    ThreadLocalsRow
  >;

  StorageTableBuilder& addVelocity2D(StorageTableBuilder& table) {
    return PhysicsTableBuilder::addVelocity(table, math::AxisFlags::XY().addA());
  }

  StorageTableBuilder& addVelocity25D(StorageTableBuilder& table) {
    return PhysicsTableBuilder::addVelocity(table, math::AxisFlags::XYZA());
  }

  template<class C, class T>
  StorageTableBuilder& addIf(StorageTableBuilder& table) {
    if(table->contains<C>()) {
      table.addRows<T>();
    }
    return table;
  }

  StorageTableBuilder& addGameplayCopy(StorageTableBuilder& table) {
    addIf<Tags::LinVelXRow, Tags::GLinVelXRow>(table);
    addIf<Tags::LinVelYRow, Tags::GLinVelYRow>(table);
    addIf<Tags::AngVelRow, Tags::GAngVelRow>(table);
    return table;
  }

  StorageTableBuilder& addGameplayImpulse(StorageTableBuilder& table) {
    addIf<Tags::LinVelXRow, Tags::GLinImpulseXRow>(table);
    addIf<Tags::LinVelYRow, Tags::GLinImpulseYRow>(table);
    addIf<Tags::LinVelZRow, Tags::GLinImpulseZRow>(table);
    addIf<Tags::AngVelRow, Tags::GAngImpulseRow>(table);
    return table;
  }

  StorageTableBuilder& addRenderable(StorageTableBuilder& table, const RenderableOptions& ops) {
    assert(ops.sharedTexture && "Individual textures not currently supported");
    if(ops.sharedTexture) {
      table.addRows<SharedTextureRow>();
    }
    if(ops.sharedMesh) {
      table.addRows<SharedMeshRow>();
    }
    else {
      table.addRows<MeshRow>();
    }
    //Currently this plus the presence of a model and mesh are what the renderer look for to render any type of mesh
    return table.addRows<Row<CubeSprite>>();
  }

  StorageTableBuilder createDynamicPhysicsObjects() {
    StorageTableBuilder table;
    Transform::addTransform2D(table);
    addVelocity2D(table);
    PhysicsTableBuilder::addCollider(table);
    PhysicsTableBuilder::addRigidbody(table);
    PhysicsTableBuilder::addAcceleration(table, math::AxisFlags::Y());
    addRenderable(table, {});
    table.addRows<
      Tags::DynamicPhysicsObjectsTag,
      SceneNavigator::IsClearedWithSceneTag,
      Narrowphase::SharedThicknessRow,
      Shapes::RectangleRow
    >().setStable().setTableName({ "Physics Objects" });
    addGameplayCopy(table);
    addGameplayImpulse(table);
    return table;
  }

  StorageTableBuilder createDynamicPhysicsObjectsWithMotor() {
    StorageTableBuilder table;
    Transform::addTransform2D(table);
    addVelocity25D(table);
    PhysicsTableBuilder::addCollider(table);
    PhysicsTableBuilder::addRigidbody(table);
    PhysicsTableBuilder::addAutoManagedJoint(table);
    addRenderable(table, {});
    table.addRows<
      Tags::DynamicPhysicsObjectsWithMotorTag,
      SceneNavigator::IsClearedWithSceneTag,
      Narrowphase::SharedThicknessRow,
      Shapes::RectangleRow
    >().setStable().setTableName({ "Dynamic With Motor" });
    return table;
  }

  StorageTableBuilder createDynamicPhysicsObjectsWithZ() {
    StorageTableBuilder table;
    Transform::addTransform25D(table);
    addVelocity25D(table);
    PhysicsTableBuilder::addCollider(table);
    PhysicsTableBuilder::addRigidbody(table);
    addRenderable(table, {});
    table.addRows<
      Tags::DynamicPhysicsObjectsWithZTag,
      SceneNavigator::IsClearedWithSceneTag,
      Narrowphase::SharedThicknessRow,
      Shapes::RectangleRow
    >().setStable().setTableName({ "Physics Objects Z" });
    addGameplayCopy(table);
    addGameplayImpulse(table);
    return table;
  }

  StorageTableBuilder createConvexPhysicsObjects() {
    StorageTableBuilder table;
    Transform::addTransform25D(table);
    addVelocity25D(table);
    PhysicsTableBuilder::addCollider(table);
    PhysicsTableBuilder::addRigidbody(table);
    PhysicsTableBuilder::addAcceleration(table, math::AxisFlags::Z());
    addRenderable(table, RenderableOptions{
      .sharedTexture = true,
      .sharedMesh = false,
    });
    table.addRows<
      Tags::DynamicPhysicsObjectsTag,
      SceneNavigator::IsClearedWithSceneTag,
      Narrowphase::SharedThicknessRow,
      Shapes::MeshReferenceRow
    >().setStable().setTableName({ "Physics Convex" });
    return table;
  }

  StorageTableBuilder createFragmentSeekingGoal() {
    StorageTableBuilder table;
    Transform::addTransform2DNoScale(table);
    addVelocity2D(table);
    PhysicsTableBuilder::addAutoManagedCustomJoint(table);
    PhysicsTableBuilder::addCollider(table);
    PhysicsTableBuilder::addRigidbody(table);
    addGameplayCopy(table);
    addGameplayImpulse(table);
    addRenderable(table, {});
    table.addRows<
      SceneNavigator::IsClearedWithSceneTag,
      FragmentSeekingGoalTagRow,
      SharedMassObjectTableTag,

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

      Narrowphase::SharedThicknessRow,
      Shapes::RectangleRow,

      IsFragment
    >().setStable().setTableName({ "Active Fragments" });
    return table;
  }

  StorageTableBuilder& addImmobile(StorageTableBuilder& table) {
    return PhysicsTableBuilder::addImmobile(table);
  }

  StorageTableBuilder createCompletedFragments() {
    StorageTableBuilder table;
    Transform::addTransform2DNoScale(table);
    PhysicsTableBuilder::addCollider(table);
    PhysicsTableBuilder::addRigidbodySharedMass(table);
    addImmobile(table);
    addGameplayCopy(table);
    addRenderable(table, {});
    table.addRows<
      SceneNavigator::IsClearedWithSceneTag,
      FragmentBurstStatEffect::CanTriggerFragmentBurstRow,
      FragmentGoalFoundTableTag,

      Narrowphase::SharedThicknessRow,
      Shapes::RectangleRow,

      IsFragment
    >().setStable().setTableName({ "Completed Fragments" });
    return table;
  }

  StorageTableBuilder createTerrain() {
    StorageTableBuilder table;
    Transform::addTransform25D(table);
    addImmobile(table);
    PhysicsTableBuilder::addCollider(table);
    PhysicsTableBuilder::addRigidbodySharedMass(table);
    addGameplayCopy(table);
    addRenderable(table, {});
    table.addRows<
      SceneNavigator::IsClearedWithSceneTag,
      FragmentBurstStatEffect::CanTriggerFragmentBurstRow,
      Tags::TerrainRow,
      Shapes::RectangleRow,
      Narrowphase::SharedThicknessRow
    >().setStable().setTableName({ "Terrain" });
    return table;
  }

  StorageTableBuilder createMeshTerrain() {
    StorageTableBuilder table;
    Transform::addTransform25D(table);
    PhysicsTableBuilder::addStaticTriangleMesh(table);
    PhysicsTableBuilder::addCollider(table);
    PhysicsTableBuilder::addRigidbodySharedMass(table);
    addRenderable(table, RenderableOptions{ .sharedMesh = false });
    table.addRows<
      SceneNavigator::IsClearedWithSceneTag,
      FragmentBurstStatEffect::CanTriggerFragmentBurstRow,
      Tags::TerrainRow,
      Narrowphase::SharedThicknessRow
    >().setStable().setTableName({ "Terrain Mesh" });
    return table;
  }

  StorageTableBuilder createInvisibleTerrain() {
    StorageTableBuilder table;
    Transform::addTransform25D(table);
    addImmobile(table);
    PhysicsTableBuilder::addCollider(table);
    PhysicsTableBuilder::addRigidbodySharedMass(table);
    addGameplayCopy(table);
    table.addRows<
      SceneNavigator::IsClearedWithSceneTag,
      FragmentBurstStatEffect::CanTriggerFragmentBurstRow,
      Tags::InvisibleTerrainRow,
      Shapes::RectangleRow,
      Narrowphase::SharedThicknessRow
    >().setStable().setTableName({ "InvisibleTerrain" });
    return table;
  }

  StorageTableBuilder createPlayer() {
    StorageTableBuilder table;
    Transform::addTransform25D(table);
    addVelocity25D(table);
    PhysicsTableBuilder::addCollider(table);
    PhysicsTableBuilder::addRigidbodySharedMass(table);
    PhysicsTableBuilder::addAutoManagedJoint(table);
    PhysicsTableBuilder::addAcceleration(table, math::AxisFlags::Z());
    addGameplayCopy(table);
    addGameplayImpulse(table);
    addRenderable(table, {});
    table.addRows<
      SceneNavigator::IsClearedWithSceneTag,
      IsPlayer,
      Tags::ElementNeedsInitRow,
      Narrowphase::SharedThicknessRow,
      Shapes::RectangleRow,
      GameInput::PlayerInputRow,
      GameInput::StateMachineRow
    >().setStable().setTableName({ "Player" });
    return table;
  }

  StorageTableBuilder createCamera() {
    StorageTableBuilder table;
    Transform::addPosXY(table).addRows<
      Row<Camera>,
      GameInput::StateMachineRow,
      GameInput::CameraDebugInputRow
    >().setStable().setTableName({ "Cameras" });
    return table;
  }

  StorageTableBuilder createFragmentSpawner() {
    StorageTableBuilder table;
    Transform::addTransform25D(table);
    addGameplayCopy(table);
    table.addRows<
      FragmentSpawner::FragmentSpawnerTagRow,
      FragmentSpawner::FragmentSpawnerCountRow,
      FragmentSpawner::FragmentSpawnStateRow,
      //Atypical use of these not to render itself but to pass down to the created fragments
      SharedTextureRow,
      SharedMeshRow,
      Narrowphase::CollisionMaskRow
    >().setStable().setTableName({ "FragmentSpawner" });
    return table;
  }

  void create(RuntimeDatabaseArgs& args) {
    createGlobalTable().finalize(args);
    SpatialQuery::createSpatialQueryTables(args);
    DBReflect::addTable<BroadphaseTable>(args);
    DBReflect::addTable<SP::SpatialPairsTable>(args);
    createTerrain().finalize(args);
    createMeshTerrain().finalize(args);
    createInvisibleTerrain().finalize(args);
    createFragmentSeekingGoal().finalize(args);
    createCompletedFragments().finalize(args);
    createPlayer().finalize(args);
    createCamera().finalize(args);
    DBReflect::addTable<DebugLineTable>(args);
    DBReflect::addTable<DebugTextTable>(args);
    createTargetPosTable().finalize(args);
    createDynamicPhysicsObjects().finalize(args);
    createDynamicPhysicsObjectsWithZ().finalize(args);
    createDynamicPhysicsObjectsWithMotor().finalize(args);
    createConvexPhysicsObjects().finalize(args);
    createFragmentSpawner().finalize(args);

    StatEffect::createDatabase(args);
    Scenes::createImportedSceneDB(args);
    Scenes::createLoadingSceneDB(args);
    Loader::createDB(args);
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
  void setName(IAppBuilder& builder, TableName::TableName name) {
    TableName::setName<Filter...>(builder, std::move(name));
  }

  //TODO: a nicer way to expose this may be for the physics module to expose a method that takes a StorageTableBuilder that can configure these as they're added.
  template<class... Filter>
  void configureSelfMotor(IAppBuilder& builder) {
    auto task = builder.createTask();
    auto q = task.query<Constraints::TableConstraintDefinitionsRow, const Filter...>();
    auto res = task.getResolver<const Constraints::CustomConstraintRow>();
    task.setCallback([q, res](AppTaskArgs&) mutable {
      for(size_t t = 0; t < q.size(); ++t) {
        auto& definitions = q.get<0>(t);
        Constraints::Definition def;
        if(res->tryGetRow<const Constraints::CustomConstraintRow>(q[t])) {
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
    configureSelfMotor<Tags::DynamicPhysicsObjectsWithMotorTag>(builder);
    configureSelfMotor<FragmentSeekingGoalTagRow>(builder);

    const auto defaultQuadMass = Mass::computeQuadMass(Mass::Quad{ .fullSize = glm::vec2{ 1.0f } }).body;
    setDefaultValue<Narrowphase::CollisionMaskRow>(builder, "setDefault Mask", uint8_t(~0));
    setDefaultValue<ConstraintSolver::ConstraintMaskRow>(builder, "setDefault Constraint Mask", ConstraintSolver::MASK_SOLVE_ALL);
    //Fragments in particular start opaque then reveal the texture as they take damage
    setDefaultValue<Tint, const IsFragment>(builder, "setDefault Tint", glm::vec4(0, 0, 0, 1));
    setDefaultValue<AccelZ>(builder, "set acceleration", -0.01f);
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
    , physicsObjsWithZ{ getOrAssertTable<Tags::DynamicPhysicsObjectsWithZTag>(task) }
    , fragmentSpawner{ getOrAssertTable<FragmentSpawner::FragmentSpawnerTagRow>(task) }
  {
  }

  Tables createTables(RuntimeDatabase& db) {
    RuntimeDatabaseTaskBuilder builder{ db };
    builder.discard();
    return { builder };
  }

  Tables::Tables(RuntimeDatabase& db)
    : Tables{ createTables(db) } {
  }

  Tables::Tables(AppTaskArgs& args)
    : Tables{ args.getLocalDB() } {
  }
}
