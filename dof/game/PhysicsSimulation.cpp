#include "Precompile.h"
#include "PhysicsSimulation.h"

#include "Simulation.h"
#include "TableAdapters.h"
#include "DebugDrawer.h"

namespace PhysicsSimulation {
  using namespace Tags;
  using PosX = FloatRow<Pos, X>;
  using PosY = FloatRow<Pos, Y>;
  using LinVelX = FloatRow<LinVel, X>;
  using LinVelY = FloatRow<LinVel, Y>;
  using AngVel = FloatRow<AngVel, Angle>;
  using RotX = FloatRow<Rot, CosAngle>;
  using RotY = FloatRow<Rot, SinAngle>;
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
      const bool drawCollisionPairs = config.drawCollisionPairs;
      const bool drawContacts = config.drawContacts;
      for(size_t t = 0; t < narrowphase.size(); ++t) {
        auto rows = narrowphase.get(t);
        const size_t size = std::get<0>(rows)->size();
        if(drawCollisionPairs) {
          const auto& ax = *std::get<0>(rows);
          const auto& ay = *std::get<1>(rows);
          const auto& bx = *std::get<2>(rows);
          const auto& by = *std::get<3>(rows);
          for(size_t i = 0; i < size; ++i) {
            DebugDrawer::drawLine(debug, glm::vec2(ax.at(i), ay.at(i)), glm::vec2(bx.at(i), by.at(i)), glm::vec3(0.0f, 1.0f, 0.0f));
          }
        }

        if(drawContacts) {
          for(size_t i = 0; i < size; ++i) {
            float overlapOne = std::get<const COne::Overlap*>(rows)->at(i);
            float overlapTwo = std::get<const CTwo::Overlap*>(rows)->at(i);
            glm::vec2 posA = TableAdapters::read(i, *std::get<const ObjA::PosX*>(rows), *std::get<const ObjA::PosY*>(rows));
            glm::vec2 posB = TableAdapters::read(i, *std::get<const ObjB::PosX*>(rows), *std::get<const ObjB::PosY*>(rows));
            glm::vec2 contactOne = TableAdapters::read(i, *std::get<const COne::PosX*>(rows), *std::get<const COne::PosY*>(rows));
            glm::vec2 contactTwo = TableAdapters::read(i, *std::get<const CTwo::PosX*>(rows), *std::get<const CTwo::PosY*>(rows));
            glm::vec2 normal = TableAdapters::read(i, *std::get<const SharedNormal::X*>(rows), *std::get<const SharedNormal::Y*>(rows));
            if(overlapOne >= 0.0f) {
              DebugDrawer::drawLine(debug, posA, contactOne, glm::vec3(1.0f, 0.0f, 0.0f));
              DebugDrawer::drawLine(debug, contactOne, contactOne + normal*0.25f, glm::vec3(0.0f, 1.0f, 0.0f));
              DebugDrawer::drawLine(debug, contactOne, contactOne + normal*overlapOne, glm::vec3(1.0f, 1.0f, 0.0f));
            }
            if(overlapTwo >= 0.0f) {
              DebugDrawer::drawLine(debug, posA, contactTwo, glm::vec3(1.0f, 0.0f, 1.0f));
              DebugDrawer::drawLine(debug, contactTwo, contactTwo + normal*0.25f, glm::vec3(0.0f, 1.0f, 1.0f));
              DebugDrawer::drawLine(debug, contactTwo, contactTwo + normal*overlapTwo, glm::vec3(1.0f, 1.0f, 1.0f));
            }
          }
        }
      }

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

  void init(GameDB game) {
    Queries::viewEachRow(game.db, [](SweepNPruneBroadphase::BroadphaseKeys& keys) {
      keys.mDefaultValue = Broadphase::SweepGrid::EMPTY_KEY;
    });
  }

  void initFromConfig(GameDB game) {
    const Config::PhysicsConfig& config = *TableAdapters::getConfig(game).physics;
    auto& grid = std::get<SharedRow<Broadphase::SweepGrid::Grid>>(std::get<BroadphaseTable>(game.db.mTables).mRows).at();
    grid.definition.bottomLeft = { config.broadphase.bottomLeftX, config.broadphase.bottomLeftY };
    grid.definition.cellSize = { config.broadphase.cellSizeX, config.broadphase.cellSizeY };
    grid.definition.cellsX = config.broadphase.cellCountX;
    grid.definition.cellsY = config.broadphase.cellCountY;
    Broadphase::SweepGrid::init(grid);
  }

  void updatePhysics(IAppBuilder& builder) {
    PhysicsAliases aliases;

    using FloatAlias = QueryAlias<Row<float>>;
    aliases.posX = FloatAlias::create<FloatRow<Pos, X>>();
    aliases.posY = FloatAlias::create<FloatRow<Pos, Y>>();
    aliases.rotX = FloatAlias::create<FloatRow<Rot, CosAngle>>();
    aliases.rotY = FloatAlias::create<FloatRow<Rot, SinAngle>>();
    aliases.linVelX = FloatAlias::create<FloatRow<LinVel, X>>();
    aliases.linVelY = FloatAlias::create<FloatRow<LinVel, X>>();
    aliases.angVel = FloatAlias::create<FloatRow<AngVel, Angle>>();
    aliases.broadphaseMinX = FloatAlias::create<SpatialQuery::MinX>();
    aliases.broadphaseMaxX = FloatAlias::create<SpatialQuery::MaxX>();
    aliases.broadphaseMinY = FloatAlias::create<SpatialQuery::MinY>();
    aliases.broadphaseMaxY = FloatAlias::create<SpatialQuery::MaxY>();

    auto temp = builder.createTask();
    const Config::PhysicsConfig& config = temp.query<SharedRow<Config::GameConfig>>().tryGetSingletonElement()->physics;
    temp.discard();

    //SpatialQuery::physicsUpdateBoundaries(game);
    Physics::applyDampingMultiplier(builder, aliases, config.linearDragMultiplier, config.angularDragMultiplier);
    SweepNPruneBroadphase::updateBroadphase(builder, _getBoundariesConfig(builder));
    Physics::updateNarrowphase(builder, aliases);
    //SpatialQuery::physicsProcessQueries(game);
    //ConstraintsTableBuilder::build(db, changedPairs, TableAdapters::getStableMappings({ db }), constraintsMappings, physicsTables, config);
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

  /*
  TaskRange preProcessEvents(GameDB db) {
    auto root = TaskNode::create([db](...) {
      auto resolver = TableResolver<PosX, PosY, SweepNPruneBroadphase::BroadphaseKeys, StableIDRow, IsImmobile>::create(db.db);
      const DBEvents& events = Events::getPublishedEvents(db);
      auto& grid = std::get<SharedRow<Broadphase::SweepGrid::Grid>>(std::get<BroadphaseTable>(db.db.mTables).mRows).at();
      SweepNPruneBroadphase::preProcessEvents(events, grid, resolver, _getBoundariesConfig(db), GameDatabase::getDescription());
    });
    return TaskBuilder::addEndSync(root);
  }

  TaskRange postProcessEvents(GameDB db) {
    auto root = TaskNode::create([db](...) {
      auto resolver = TableResolver<PosX, PosY, SweepNPruneBroadphase::BroadphaseKeys, StableIDRow, IsImmobile>::create(db.db);
      const DBEvents& events = Events::getPublishedEvents(db);
      auto& grid = std::get<SharedRow<Broadphase::SweepGrid::Grid>>(std::get<BroadphaseTable>(db.db.mTables).mRows).at();
      SweepNPruneBroadphase::postProcessEvents(events, grid, resolver, _getBoundariesConfig(db), GameDatabase::getDescription(), TableAdapters::getStableMappings(db));
    });
    return TaskBuilder::addEndSync(root);
  }
  */
}