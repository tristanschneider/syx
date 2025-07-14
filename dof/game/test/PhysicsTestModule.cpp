#include "Precompile.h"

#include "test/PhysicsTestModule.h"

#include <AppBuilder.h>
#include <GameDatabase.h>
#include <GraphicsTables.h>
#include <IAppModule.h>
#include <loader/ReflectionModule.h>
#include <SceneNavigator.h>
#include <TLSTaskImpl.h>
#include <TableName.h>
#include <Narrowphase.h>
#include <shapes/Mesh.h>
#include <shapes/Rectangle.h>
#include <shapes/ShapeRegistry.h>
#include <SpatialQueries.h>
#include <TransformResolver.h>
#include <PhysicsSimulation.h>
#include <RowTags.h>
#include <DebugDrawer.h>
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include <format>
#include <PhysicsTableBuilder.h>

namespace PhysicsTestModule {
  //Objects with collision being checked for correctness
  struct ValidationTargetRow : Loader::PersistentElementRefRow {
    static constexpr std::string_view KEY = "ValidationTarget";
  };
  //Markers indicating the correct collision information for objects in the ValidationTargetRow
  struct ValidationMarkerRow : TagRow{};

  class DebugLogger : public ILogger {
  public:
    DebugLogger(RuntimeDatabaseTaskBuilder& task)
      : drawer{ DebugDrawer::createDebugDrawer(task) }
    {}

    void logValidationError(const ValidationStats&, const glm::vec2& pos, ValidationError error) {
      const glm::vec2 scale{ 1.f };
      drawer->drawAABB(pos - scale, pos + scale, glm::vec3{ 1, 1, 1 });
      drawer->drawText(pos - glm::vec2{ 0, -scale.y }, std::move(error.message));
    }

    void logValidationSuccess(const ValidationStats&) {}

    void logResults(const ValidationStats& stats) {
      const glm::vec2 pos{ 0, -20 };
      drawer->drawText(glm::vec2{ 0, -5 }, std::format("{}/{} succeeded", stats.pass, stats.total()));
    }

    std::unique_ptr<DebugDrawer::IDebugDrawer> drawer;
  };

  struct ValidateTask {
    struct Group {
      void init(LogFactory&& f) {
        factory = std::move(f);
      }

      void init(RuntimeDatabaseTaskBuilder& task) {
        logger = factory(task);
      }

      LogFactory factory;
      std::shared_ptr<ILogger> logger;
    };

    void init(RuntimeDatabaseTaskBuilder& task) {
      toValidate = task;
      shapes = ShapeRegistry::get(task)->createShapeClassifier(task);
      spatialQuery = SpatialQuery::createReader(task);
      drawer = DebugDrawer::createDebugDrawer(task);
      res = task.getRefResolver();
    }

    void init(const Group& group) {
      logger = group.logger;
    }

    static ValidationError getMarkerMatchError(const glm::vec2& pos, const SpatialQuery::Result& result, const ShapeRegistry::Mesh& mesh) {
      const SpatialQuery::ContactXY* contact = std::get_if<SpatialQuery::ContactXY>(&result.contact);
      if(!contact) {
        return ValidationError{ .message = "Unexpected contact type" };
      }
      constexpr float threshold = 0.01f;
      constexpr float threshold2 = threshold*threshold;
      float closest = std::numeric_limits<float>::max();
      //Assume that valid collision points are vertices on the mesh.
      //More forgiving would be to project the point onto the boundary.
      for(size_t i = 0; i < contact->size; ++i) {
        const glm::vec2 c = contact->points[i].point + pos;
        for(const glm::vec2& m : mesh.points) {
          closest = glm::min(closest, glm::distance2(c, mesh.modelToWorld.transformPoint(m)));
          if(closest <= threshold2) {
            return {};
          }
        }
      }

      return ValidationError{
        .message = std::format("No matching contact found, closest distance was {}", closest)
      };
    }

    void displayValidationError(ValidationStats& stats, size_t t, size_t i, ValidationError error) {
      ++stats.fail;
      if(logger) {
        auto [targets, ids, x, y] = toValidate.get(t);
        const glm::vec2 pos{ x->at(i), y->at(i) };
        logger->logValidationError(stats, pos, std::move(error));
      }
    }

    void displayValidationSuccess(ValidationStats& stats) {
      ++stats.pass;
      if(logger) {
        logger->logValidationSuccess(stats);
      }
    }

    void displayResults(const ValidationStats& stats) {
      if(logger) {
        logger->logResults(stats);
      }
    }

    void execute() {
      ValidationStats stats;
      for(size_t t = 0; t < toValidate.size(); ++t) {
        auto [targets, targetIDS, x, y] = toValidate.get(t);
        for(size_t i = 0; i < targets.size; ++i) {
          const UnpackedDatabaseElementID marker = res.unpack(targets->at(i).tryGetRef());
          const UnpackedDatabaseElementID self = res.unpack(targetIDS->at(i));
          const ShapeRegistry::BodyType shapeType = shapes->classifyShape(marker);
          const ShapeRegistry::Mesh* markerMesh = std::get_if<ShapeRegistry::Mesh>(&shapeType.shape);
          const glm::vec2 pos{ x->at(i), y->at(i) };
          if(!markerMesh) {
            displayValidationError(stats, t, i, ValidationError{ .message = "Unable to resolve marker" });
            continue;
          }

          spatialQuery->begin(targetIDS->at(i));
          size_t count{};
          bool matchFound{};
          while(const SpatialQuery::Result* result = spatialQuery->tryIterate()) {
            ++count;
            if(!getMarkerMatchError(pos, *result, *markerMesh)) {
              matchFound = true;
              break;
            }
          }

          if(matchFound) {
            displayValidationSuccess(stats);
          }
          else {
            ValidationError error;
            if(count) {
              error.message = "Collisions found, but none matching marker";
            }
            else {
              error.message = "No collision found";
            }
            displayValidationError(stats, t, i, std::move(error));
          }
        }
      }

      if(stats.total()) {
        displayResults(stats);
      }
    }

    QueryResult<const ValidationTargetRow, const StableIDRow, const Tags::GPosXRow, const Tags::GPosYRow> toValidate;
    std::shared_ptr<ShapeRegistry::IShapeClassifier> shapes;
    std::shared_ptr<SpatialQuery::IReader> spatialQuery;
    std::unique_ptr<DebugDrawer::IDebugDrawer> drawer;
    std::shared_ptr<ILogger> logger;
    ElementRefResolver res;
  };

  class Module : public IAppModule {
  public:
    Module(LogFactory _factory)
      : factory{ std::move(_factory) }
    {
    }

    static void addBase(StorageTableBuilder& table) {
      table.setStable();
      table.addRows<
        SceneNavigator::IsClearedWithSceneTag,
        Narrowphase::SharedThicknessRow
      >();
      GameDatabase::addTransform25D(table);
      GameDatabase::addRenderable(table, GameDatabase::RenderableOptions{
        .sharedTexture = true,
        .sharedMesh = false,
      });
      GameDatabase::addGameplayCopy(table);
    }

    void createDatabase(RuntimeDatabaseArgs& args) {
      std::invoke([]{
        StorageTableBuilder table;
        addBase(table);
        PhysicsTableBuilder::addCollider(table);
        table.addRows<
          ValidationTargetRow,
          Shapes::SharedRectangleRow
        >().setTableName({ "CollisionToValidate" });
        return table;
      }).finalize(args);

      std::invoke([]{
        StorageTableBuilder table;
        addBase(table);
        table.addRows<
          ValidationMarkerRow,
          Shapes::MeshReferenceRow
        >().setTableName({ "CollisionMarkers" });
        return table;
      }).finalize(args);
    }

    void init(IAppBuilder& builder) {
      Reflection::registerLoaders(builder, std::make_unique<Reflection::ObjIDLoader<ValidationTargetRow>>());
    }

    void update(IAppBuilder& builder) {
      builder.submitTask(TLSTask::createWithArgs<ValidateTask, ValidateTask::Group>("validate", std::move(factory)));
    }

    LogFactory factory;
  };

  std::unique_ptr<IAppModule> create(LogFactory factory) {
    return std::make_unique<Module>(std::move(factory));
  }

  LogFactory createDebugLogger() {
    return [](RuntimeDatabaseTaskBuilder& task) {
      return std::make_unique<DebugLogger>(task);
    };
  }
}