#include "Precompile.h"
#include "shapes/Circle.h"
#include "AppBuilder.h"

namespace Shapes {
  struct IndividualCircleShape : ShapeRegistry::IShapeImpl {
    std::vector<TableID> queryTables(IAppBuilder& builder) const final {
      return builder.queryTables<const CircleRow>().getMatchingTableIDs();
    }

    struct Classifier : ShapeRegistry::IShapeClassifier {
      Classifier(RuntimeDatabaseTaskBuilder& task, ITableResolver& res)
        : resolver{ res }
      {
        //Log the dependency with get, but use the shared resolver
        task.getResolver(row);
      }

      ShapeRegistry::BodyType classifyShape(const UnpackedDatabaseElementID& id) final {
        if(const ShapeRegistry::Circle* circle = resolver.tryGetOrSwapRowElement(row, id)) {
          return { { *circle } };
        }
        return {};
      }

      ITableResolver& resolver;
      CachedRow<const CircleRow> row;
    };

    std::shared_ptr<ShapeRegistry::IShapeClassifier> createShapeClassifier(RuntimeDatabaseTaskBuilder& task, ITableResolver& resolver) const final {
      return std::make_shared<Classifier>(task, resolver);
    }

    void writeBoundaries(IAppBuilder& builder, ShapeRegistry::BroadphaseBounds& bounds) const final {
      auto task = builder.createTask();
      task.setName("write circle indiv bounds");
      task.logDependency({ bounds.requiredDependency });

      auto query = task.query<const CircleRow>(bounds.table);
      task.setCallback([query, &bounds](AppTaskArgs&) mutable {
        auto [circles] = query.get(0);
        const size_t s = circles->size();
        bounds.minX.resize(s);
        bounds.minY.resize(s);
        bounds.maxX.resize(s);
        bounds.maxY.resize(s);
        for(size_t i = 0; i < circles->size(); ++i) {
          const ShapeRegistry::Circle& circle = circles->at(i);
          bounds.minX[i] = circle.pos.x - circle.radius;
          bounds.maxX[i] = circle.pos.x + circle.radius;
          bounds.minY[i] = circle.pos.y - circle.radius;
          bounds.maxY[i] = circle.pos.y + circle.radius;
        }
      });

      builder.submitTask(std::move(task));
    }
  };

  std::unique_ptr<ShapeRegistry::IShapeImpl> createIndividualCircle() {
    return std::make_unique<IndividualCircleShape>();
  }
}
