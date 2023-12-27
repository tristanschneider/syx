#include "Precompile.h"
#include "PhysicsSimulation.h"

#include "Simulation.h"
#include "TableAdapters.h"
#include "DebugDrawer.h"
#include "Narrowphase.h"

namespace PhysicsSimulation {
  using PosX = FloatRow<Tags::Pos, Tags::X>;
  using PosY = FloatRow<Tags::Pos, Tags::Y>;
  using LinVelX = FloatRow<Tags::LinVel, Tags::X>;
  using LinVelY = FloatRow<Tags::LinVel, Tags::Y>;
  using AngVel = FloatRow<Tags::AngVel, Tags::Angle>;
  using RotX = FloatRow<Tags::Rot, Tags::CosAngle>;
  using RotY = FloatRow<Tags::Rot, Tags::SinAngle>;
  //For now use the existence of this row to indicate that the given object should participate in collision
  using HasCollision = Row<CubeSprite>;

  SweepNPruneBroadphase::BoundariesConfig _getBoundariesConfig(IAppBuilder& builder) {
    auto temp = builder.createTask();
    const Config::GameConfig& cfg = *temp.query<const SharedRow<Config::GameConfig>>().tryGetSingletonElement();
    temp.discard();
    SweepNPruneBroadphase::BoundariesConfig result;
    result.mPadding = cfg.physics.broadphase.cellPadding;
    return result;
  }

  void _debugUpdate(IAppBuilder& builder, const Config::PhysicsConfig& config) {
    auto task = builder.createTask();
    task.setName("physics debug");
    DebugLineAdapter debug = TableAdapters::getDebugLines(task);
    using ObjA = NarrowphaseData<PairA>;
    using ObjB = NarrowphaseData<PairB>;
    using COne = ContactPoint<ContactOne>;
    using CTwo = ContactPoint<ContactTwo>;
    auto narrowphase = task.query<
      const ObjA::PosX, const ObjA::PosY,
      const ObjB::PosX, const ObjB::PosY,
      const COne::PosX, const COne::PosY, const COne::Overlap,
      const CTwo::PosX, const CTwo::PosY, const CTwo::Overlap,
      const SharedNormal::X, const SharedNormal::Y
    >();
    auto broadphase = task.query<const SharedRow<Broadphase::SweepGrid::Grid>>();

    task.setCallback([debug, narrowphase, broadphase, &config](AppTaskArgs&) mutable {
      if(config.broadphase.draw) {
        for(size_t t = 0; t < broadphase.size(); ++t) {
          const Broadphase::SweepGrid::Grid& grid = broadphase.get<0>(t).at();
          for(size_t i = 0; i < grid.cells.size(); ++i) {
            const Broadphase::Sweep2D& sweep = grid.cells[i];
            const glm::vec2 basePos{ static_cast<float>(i % grid.definition.cellsX), static_cast<float>(i / grid.definition.cellsX) };
            const glm::vec2 min = grid.definition.bottomLeft + basePos*grid.definition.cellSize;
            const glm::vec2 max = min + grid.definition.cellSize;
            const glm::vec2 center = (min + max) * 0.5f;
            glm::vec3 color{ 1.0f, 0.0f, 0.0f };
            DebugDrawer::drawLine(debug, min, { max.x, min.y }, color);
            DebugDrawer::drawLine(debug, { max.x, min.y }, max, color);
            DebugDrawer::drawLine(debug, max, { min.x, max.y }, color);
            DebugDrawer::drawLine(debug, min, { min.x, max.y }, color);

            for(size_t b = 0; b < sweep.bounds[0].size(); ++b) {
              const auto& x = sweep.bounds[0][b];
              const auto& y = sweep.bounds[1][b];
              if(x.first == Broadphase::Sweep2D::REMOVED) {
                continue;
              }
              color = { 0.0f, 1.0f, 1.0f };
              DebugDrawer::drawLine(debug, { x.first, min.y }, { x.first, max.y }, color);
              DebugDrawer::drawLine(debug, { x.second, min.y }, { x.second, max.y }, color);
              DebugDrawer::drawLine(debug, { min.x, y.first }, { max.x, y.first }, color);
              DebugDrawer::drawLine(debug, { min.x, y.second }, { max.x, y.second }, color);
              color = { 0.0f, 1.0f, 0.0f };
              DebugDrawer::drawLine(debug, center, { (x.first + x.second)*0.5f, (y.first + y.second)*0.5f }, color);
            }
          }
        }
      }
    });

    builder.submitTask(std::move(task));
  }

  Narrowphase::UnitCubeDefinition getUnitCubeDefinition() {
    return {
      ConstFloatQueryAlias::create<const FloatRow<Tags::Pos, Tags::X>>(),
      ConstFloatQueryAlias::create<const FloatRow<Tags::Pos, Tags::Y>>(),
      ConstFloatQueryAlias::create<const FloatRow<Tags::Rot, Tags::CosAngle>>(),
      ConstFloatQueryAlias::create<const FloatRow<Tags::Rot, Tags::SinAngle>>()
    };
  }

  void init(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("init physics");
    auto query = task.query<SweepNPruneBroadphase::BroadphaseKeys>();
    task.setCallback([query](AppTaskArgs&) mutable {
      query.forEachRow([](auto& row) { row.mDefaultValue = Broadphase::SweepGrid::EMPTY_KEY; });
    });
    builder.submitTask(std::move(task));
  }

  void initFromConfig(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("init physics from config");
    const Config::PhysicsConfig* config = TableAdapters::getPhysicsConfig(task);
    auto query = task.query<SharedRow<Broadphase::SweepGrid::Grid>>();
    task.setCallback([query, config](AppTaskArgs&) mutable {
      for(size_t t = 0; t < query.size(); ++t) {
        auto& grid = query.get<0>(t).at();
        grid.definition.bottomLeft = { config->broadphase.bottomLeftX, config->broadphase.bottomLeftY };
        grid.definition.cellSize = { config->broadphase.cellSizeX, config->broadphase.cellSizeY };
        grid.definition.cellsX = config->broadphase.cellCountX;
        grid.definition.cellsY = config->broadphase.cellCountY;
        Broadphase::SweepGrid::init(grid);
      }
    });
    builder.submitTask(std::move(task));
  }

  PhysicsAliases getPhysicsAliases() {
    PhysicsAliases aliases;

    using FloatAlias = QueryAlias<Row<float>>;
    aliases.posX = FloatAlias::create<FloatRow<Tags::Pos, Tags::X>>();
    aliases.posY = FloatAlias::create<FloatRow<Tags::Pos, Tags::Y>>();
    aliases.rotX = FloatAlias::create<FloatRow<Tags::Rot, Tags::CosAngle>>();
    aliases.rotY = FloatAlias::create<FloatRow<Tags::Rot, Tags::SinAngle>>();
    aliases.linVelX = FloatAlias::create<FloatRow<Tags::LinVel, Tags::X>>();
    aliases.linVelY = FloatAlias::create<FloatRow<Tags::LinVel, Tags::Y>>();
    aliases.angVel = FloatAlias::create<FloatRow<Tags::AngVel, Tags::Angle>>();
    aliases.broadphaseMinX = FloatAlias::create<SpatialQuery::Physics<SpatialQuery::MinX>>();
    aliases.broadphaseMaxX = FloatAlias::create<SpatialQuery::Physics<SpatialQuery::MaxX>>();
    aliases.broadphaseMinY = FloatAlias::create<SpatialQuery::Physics<SpatialQuery::MinY>>();
    aliases.broadphaseMaxY = FloatAlias::create<SpatialQuery::Physics<SpatialQuery::MaxY>>();

    using TagAlias = QueryAlias<TagRow>;
    aliases.isImmobile = TagAlias::create<IsImmobile>();

    return aliases;
  }

  void updatePhysics(IAppBuilder& builder) {
    auto temp = builder.createTask();
    const Config::PhysicsConfig& config = temp.query<SharedRow<Config::GameConfig>>().tryGetSingletonElement()->physics;
    temp.discard();

    const PhysicsAliases aliases = getPhysicsAliases();

    SpatialQuery::physicsUpdateBoundaries(builder);
    Physics::applyDampingMultiplier(builder, aliases, config.linearDragMultiplier, config.angularDragMultiplier);
    SweepNPruneBroadphase::updateBroadphase(builder, _getBoundariesConfig(builder), aliases);

    Narrowphase::generateContactsFromSpatialPairs(builder, getUnitCubeDefinition());
    Physics::updateNarrowphase(builder, aliases);

    ConstraintsTableBuilder::build(builder, config);
    Physics::fillConstraintVelocities(builder, aliases);
    Physics::setupConstraints(builder);
    PhysicsSimulation::_debugUpdate(builder, config);
    const int solveIterations = config.solveIterations;
    //TODO: stop early if global lambda sum falls below tolerance
    for(int i = 0; i < solveIterations; ++i) {
      Physics::solveConstraints(builder, config);
    }
    Physics::storeConstraintVelocities(builder, aliases);
    Physics::integratePosition(builder, aliases);
    Physics::integrateRotation(builder, aliases);
  }

  void preProcessEvents(IAppBuilder& builder) {
    auto task = builder.createTask();
    const DBEvents& events = Events::getPublishedEvents(task);
    SweepNPruneBroadphase::preProcessEvents(task, events);
    builder.submitTask(std::move(task));
  }

  void postProcessEvents(IAppBuilder& builder) {
    auto task = builder.createTask();
    const DBEvents& events = Events::getPublishedEvents(task);
    SweepNPruneBroadphase::postProcessEvents(task, events, getPhysicsAliases(), _getBoundariesConfig(builder));
    builder.submitTask(std::move(task));
  }
}