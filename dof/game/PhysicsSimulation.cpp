#include "Precompile.h"
#include "PhysicsSimulation.h"

#include "Simulation.h"
#include "TableAdapters.h"
#include "DebugDrawer.h"
#include "Narrowphase.h"
#include "ConstraintSolver.h"
#include "Physics.h"
#include "SweepNPruneBroadphase.h"
#include "SpatialQueries.h"
#include "shapes/DefaultShapes.h"
#include "shapes/ShapeRegistry.h"
#include "shapes/Rectangle.h"
#include "Constraints.h"
#include "TransformResolver.h"
#include <module/MassModule.h>

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
    auto broadphase = task.query<const SharedRow<Broadphase::SweepGrid::Grid>>();

    task.setCallback([debug, broadphase, &config](AppTaskArgs&) mutable {
      if(config.broadphase.draw) {
        for(size_t t = 0; t < broadphase.size(); ++t) {
          const Broadphase::SweepGrid::Grid& grid = broadphase.get<0>(t).at();
          for(size_t i = 0; i < grid.cells.size(); ++i) {
            const Broadphase::Sweep2D& sweep = grid.cells[i];
            const glm::vec2 min{ sweep.axis[0].min, sweep.axis[1].min };
            const glm::vec2 max{ sweep.axis[0].max, sweep.axis[1].max };
            const glm::vec2 center = (min + max) * 0.5f;
            glm::vec3 color{ 1.0f, 0.0f, 0.0f };
            DebugDrawer::drawLine(debug, min, { max.x, min.y }, color);
            DebugDrawer::drawLine(debug, { max.x, min.y }, max, color);
            DebugDrawer::drawLine(debug, max, { min.x, max.y }, color);
            DebugDrawer::drawLine(debug, min, { min.x, max.y }, color);

            for(const auto& key : sweep.containedKeys) {
              const auto& x = grid.objects.bounds[0][key.value];
              const auto& y = grid.objects.bounds[1][key.value];
              if(x.first == Broadphase::ObjectDB::REMOVED) {
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

  Shapes::RectDefinition getRectDefinition() {
    return {
      ConstFloatQueryAlias::create<const FloatRow<Tags::Pos, Tags::X>>(),
      ConstFloatQueryAlias::create<const FloatRow<Tags::Pos, Tags::Y>>(),
      ConstFloatQueryAlias::create<const FloatRow<Tags::Rot, Tags::CosAngle>>(),
      ConstFloatQueryAlias::create<const FloatRow<Tags::Rot, Tags::SinAngle>>(),
      ConstFloatQueryAlias::create<const Tags::ScaleXRow>(),
      ConstFloatQueryAlias::create<const Tags::ScaleYRow>()
    };
  }

  std::shared_ptr<ShapeRegistry::IShapeClassifier> createShapeClassifier(RuntimeDatabaseTaskBuilder& task) {
    return ShapeRegistry::get(task)->createShapeClassifier(task);
  }

  class PhysicsBodyResolver : public IPhysicsBodyResolver {
  public:
    PhysicsBodyResolver(RuntimeDatabaseTaskBuilder& task)
      : shape(createShapeClassifier(task))
      , ids(task.getIDResolver())
      , resolver(task.getResolver(lvx, lvy, av))
    {}

    std::optional<Key> tryResolve(const ElementRef& e) final {
      if(auto resolved = ids->getRefResolver().tryUnpack(e)) {
        return *resolved;
      }
      return {};
    }

    glm::vec2 getCenter(const Key& e) final {
      return ShapeRegistry::getCenter(shape->classifyShape(e));
    }

    glm::vec2 getLinearVelocity(const Key& e) final {
      if(resolver->tryGetOrSwapAllRows(e, lvx, lvy)) {
        return TableAdapters::read(e.getElementIndex(), *lvx, *lvy);
      }
      return glm::vec2{ 0 };
    }

    float getAngularVelocity(const Key& e) final {
      const float* result = resolver->tryGetOrSwapRowElement(av, e);
      return result ? *result : 0.0f;
    }

    std::shared_ptr<Narrowphase::IShapeClassifier> shape;
    CachedRow<const FloatRow<Tags::GLinVel, Tags::X>> lvx;
    CachedRow<const FloatRow<Tags::GLinVel, Tags::Y>> lvy;
    CachedRow<const FloatRow<Tags::GAngVel, Tags::Angle>> av;
    std::shared_ptr<IIDResolver> ids;
    std::shared_ptr<ITableResolver> resolver;
  };

  std::shared_ptr<IPhysicsBodyResolver> createPhysicsBodyResolver(RuntimeDatabaseTaskBuilder& task) {
    return std::make_shared<PhysicsBodyResolver>(task);
  }

  pt::TransformResolver createTransformResolver(RuntimeDatabaseTaskBuilder& task) {
    return { task, getPhysicsAliases() };
  }

  pt::TransformResolver createGameplayTransformResolver(RuntimeDatabaseTaskBuilder& task) {
    return { task, getGameplayPhysicsAliases() };
  }

  pt::FullTransformResolver createGameplayFullTransformResolver(RuntimeDatabaseTaskBuilder& task) {
    return { task, getGameplayPhysicsAliases() };
  }

  void init(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("init physics");
    auto query = task.query<SweepNPruneBroadphase::BroadphaseKeys>();
    task.setCallback([query](AppTaskArgs&) mutable {
      query.forEachRow([](auto& row) { row.setDefaultValue(Broadphase::SweepGrid::EMPTY_KEY); });
    });
    builder.submitTask(std::move(task));

    auto temp = builder.createTask();
    temp.discard();
    ShapeRegistry::IShapeRegistry* reg = ShapeRegistry::getMutable(temp);
    Shapes::registerDefaultShapes(*reg, getRectDefinition());
    ShapeRegistry::finalizeRegisteredShapes(builder);
    Constraints::init(builder);
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
    aliases.posZ = FloatAlias::create<FloatRow<Tags::Pos, Tags::Z>>();
    aliases.rotX = FloatAlias::create<FloatRow<Tags::Rot, Tags::CosAngle>>();
    aliases.rotY = FloatAlias::create<FloatRow<Tags::Rot, Tags::SinAngle>>();
    aliases.scaleX = FloatAlias::create<Tags::ScaleXRow>();
    aliases.scaleY = FloatAlias::create<Tags::ScaleYRow>();
    aliases.linVelX = FloatAlias::create<FloatRow<Tags::LinVel, Tags::X>>();
    aliases.linVelY = FloatAlias::create<FloatRow<Tags::LinVel, Tags::Y>>();
    aliases.linVelZ = FloatAlias::create<FloatRow<Tags::LinVel, Tags::Z>>();
    aliases.angVel = FloatAlias::create<FloatRow<Tags::AngVel, Tags::Angle>>();
    aliases.broadphaseMinX = FloatAlias::create<SpatialQuery::Physics<SpatialQuery::MinX>>();
    aliases.broadphaseMaxX = FloatAlias::create<SpatialQuery::Physics<SpatialQuery::MaxX>>();
    aliases.broadphaseMinY = FloatAlias::create<SpatialQuery::Physics<SpatialQuery::MinY>>();
    aliases.broadphaseMaxY = FloatAlias::create<SpatialQuery::Physics<SpatialQuery::MaxY>>();

    return aliases;
  }

  PhysicsAliases getGameplayPhysicsAliases() {
    using F = QueryAlias<Row<float>>;
    using T = QueryAlias<TagRow>;
    return PhysicsAliases {
      .posX = F::create<FloatRow<Tags::GPos, Tags::X>>(),
      .posY = F::create<FloatRow<Tags::GPos, Tags::Y>>(),
      .rotX = F::create<FloatRow<Tags::GRot, Tags::CosAngle>>(),
      .rotY = F::create<FloatRow<Tags::GRot, Tags::SinAngle>>(),
      .scaleX = F::create<Tags::ScaleXRow>(),
      .scaleY = F::create<Tags::ScaleYRow>(),
      .linVelX = F::create<FloatRow<Tags::GLinVel, Tags::X>>(),
      .linVelY = F::create<FloatRow<Tags::GLinVel, Tags::Y>>(),
      .angVel = F::create<FloatRow<Tags::GAngVel, Tags::Angle>>(),
      .posZ = F::create<FloatRow<Tags::GPos, Tags::Z>>(),
      .linVelZ = F::create<FloatRow<Tags::GLinVel, Tags::Z>>(),
      .broadphaseMinX = F::create<SpatialQuery::Physics<SpatialQuery::MinX>>(),
      .broadphaseMinY = F::create<SpatialQuery::Physics<SpatialQuery::MinY>>(),
      .broadphaseMaxX = F::create<SpatialQuery::Physics<SpatialQuery::MaxX>>(),
      .broadphaseMaxY = F::create<SpatialQuery::Physics<SpatialQuery::MaxY>>(),
    };
  }

  void updatePhysics(IAppBuilder& builder) {
    auto temp = builder.createTask();
    const Config::PhysicsConfig& config = temp.query<SharedRow<Config::GameConfig>>().tryGetSingletonElement()->physics;

    //TODO: move to config
    static float biasTerm = ConstraintSolver::SolverGlobals::BIAS_DEFAULT;
    static float slop = ConstraintSolver::SolverGlobals::SLOP_DEFAULT;
    ConstraintSolver::SolverGlobals globals{
      &biasTerm,
      &slop
    };
    temp.discard();

    const PhysicsAliases aliases = getPhysicsAliases();
    Physics::integrateVelocity(builder, aliases);
    Physics::applyDampingMultiplier(builder, aliases, config.linearDragMultiplier, config.angularDragMultiplier);
    SweepNPruneBroadphase::updateBroadphase(builder, _getBoundariesConfig(builder), aliases);
    Constraints::update(builder, aliases, globals);

    Narrowphase::generateContactsFromSpatialPairs(builder, aliases, TableAdapters::getThreadCount(temp));
    ConstraintSolver::solveConstraints(builder, aliases, globals);

    Physics::integratePosition(builder, aliases);
    Physics::integrateRotation(builder, aliases);

    _debugUpdate(builder, config);
  }

  void preProcessEvents(IAppBuilder& builder) {
    SweepNPruneBroadphase::preProcessEvents(builder);
  }

  void postProcessEvents(IAppBuilder& builder) {
    SweepNPruneBroadphase::postProcessEvents(builder, getPhysicsAliases(), _getBoundariesConfig(builder));
    Constraints::postProcessEvents(builder);
  }
}