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

namespace PerformanceDB {
  using BroadphaseTable = SweepNPruneBroadphase::BroadphaseTable;
  using GlobalGameData = Table<
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
    Narrowphase::SharedRectangleRow,
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

  std::unique_ptr<IDatabase> create(StableElementMappings& mappings) {
    return DBReflect::merge(
      DBReflect::createDatabase<DB>(mappings),
      DBReflect::createDatabase<StatEffectDatabase>(mappings)
    );
  }

  std::unique_ptr<IDatabase> create() {
    auto mappings = std::make_unique<StableElementMappings>();
    std::unique_ptr<IDatabase> game = create(*mappings);
    return DBReflect::bundle(std::move(game), std::move(mappings));
  }
};