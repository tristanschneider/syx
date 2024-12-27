#include "PerformanceDB.h"

#include "Simulation.h"
#include "SweepNPruneBroadphase.h"
#include "Narrowphase.h"
#include "ConstraintSolver.h"
#include "GameDatabase.h"
#include "GameBuilder.h"
#include "SpatialQueries.h"
#include "SpatialPairsStorage.h"
#include "stat/AllStatEffects.h"
#include "scenes/SceneList.h"
#include "SceneNavigator.h"
#include "shapes/ShapeRegistry.h"
#include "shapes/Rectangle.h"

namespace PerformanceDB {
  using BroadphaseTable = SweepNPruneBroadphase::BroadphaseTable;
  using GlobalGameData = Table<
    ShapeRegistry::GlobalRow,
    SceneList::ScenesRow,
    SharedRow<StableElementMappings>,
    SharedRow<Scheduler>,
    SharedRow<Config::GameConfig>,
    //GameInput::GlobalMappingsRow,
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

    SweepNPruneBroadphase::BroadphaseKeys,
    Narrowphase::CollisionMaskRow,
    Shapes::SharedRectangleRow,
    ConstraintSolver::ConstraintMaskRow,
    ConstraintSolver::SharedMassRow,
    ConstraintSolver::SharedMaterialRow,

    StableIDRow
  >;

  using DB = Database<
    SpatialQuery::AABBSpatialQueriesTable,
    SpatialQuery::CircleSpatialQueriesTable,
    SpatialQuery::RaycastSpatialQueriesTable,
    BroadphaseTable,
    SP::SpatialPairsTable,
    GlobalGameData,
    GameObjectTable
  >;

  void create(RuntimeDatabaseArgs& args) {
    DBReflect::addDatabase<DB>(args);
    DBReflect::addDatabase<StatEffectDatabase>(args);
    SceneNavigator::createDB(args);
  }

  std::unique_ptr<IDatabase> create() {
    RuntimeDatabaseArgs args = DBReflect::createArgsWithMappings();
    create(args);
    return std::make_unique<RuntimeDatabase>(std::move(args));
  }
};