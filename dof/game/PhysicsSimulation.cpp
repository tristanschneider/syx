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
#include <transform/TransformResolver.h>
#include <module/MassModule.h>
#include <Physics.h>

namespace PhysicsSimulation {
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

  class PhysicsBodyResolver : public IPhysicsBodyResolver {
  public:
    PhysicsBodyResolver(RuntimeDatabaseTaskBuilder& task)
      : shape(Physics::createShapeClassifier(task))
      , ids(task.getIDResolver())
      , resolver(task.getResolver(lvx, lvy, av))
      , transformResolver{ task, {} }
    {}

    std::optional<Key> tryResolve(const ElementRef& e) final {
      if(auto resolved = ids->getRefResolver().tryUnpack(e)) {
        return *resolved;
      }
      return {};
    }

    glm::vec2 getCenter(const Key& e) final {
      return transformResolver.resolve(e).pos2();
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
    Transform::Resolver transformResolver;
    CachedRow<const FloatRow<Tags::GLinVel, Tags::X>> lvx;
    CachedRow<const FloatRow<Tags::GLinVel, Tags::Y>> lvy;
    CachedRow<const FloatRow<Tags::GAngVel, Tags::Angle>> av;
    std::shared_ptr<IIDResolver> ids;
    std::shared_ptr<ITableResolver> resolver;
  };

  std::shared_ptr<IPhysicsBodyResolver> createPhysicsBodyResolver(RuntimeDatabaseTaskBuilder& task) {
    return std::make_shared<PhysicsBodyResolver>(task);
  }

  void updatePhysics(IAppBuilder& builder) {
    auto temp = builder.createTask();
    temp.discard();
    const Config::PhysicsConfig& config = temp.query<SharedRow<Config::GameConfig>>().tryGetSingletonElement()->physics;
    //TODO: diagnostics module that physics can depend on so this moves to Physics project
    _debugUpdate(builder, config);
  }
}