#include "Precompile.h"
#include "shapes/Line.h"
#include "AppBuilder.h"

namespace Shapes {
  struct IndividualLineShape : ShapeRegistry::IShapeImpl {
    std::vector<UnpackedDatabaseElementID> queryTables(IAppBuilder& builder) const final {
      return builder.queryTables<const LineRow>().matchingTableIDs;
    }

    struct Classifier : ShapeRegistry::IShapeClassifier {
      Classifier(RuntimeDatabaseTaskBuilder& task, ITableResolver& res)
        : resolver{ res }
      {
        //Log the dependency with get, but use the shared resolver
        task.getResolver(row);
      }

      ShapeRegistry::BodyType classifyShape(const UnpackedDatabaseElementID& id) final {
        if(const ShapeRegistry::Raycast* line = resolver.tryGetOrSwapRowElement(row, id)) {
          return { { *line } };
        }
        return {};
      }

      ITableResolver& resolver;
      CachedRow<const LineRow> row;
    };

    std::shared_ptr<ShapeRegistry::IShapeClassifier> createShapeClassifier(RuntimeDatabaseTaskBuilder& task, ITableResolver& resolver) const final {
      return std::make_shared<Classifier>(task, resolver);
    }

    void writeBoundaries(IAppBuilder& builder, ShapeRegistry::BroadphaseBounds& bounds) const final {
      auto task = builder.createTask();
      task.setName("write line indiv bounds");
      task.logDependency({ bounds.requiredDependency });

      auto query = task.query<const LineRow>(bounds.table);
      task.setCallback([query, &bounds](AppTaskArgs&) mutable {
        auto [lines] = query.get(0);
        const size_t s = lines->size();
        bounds.minX.resize(s);
        bounds.minY.resize(s);
        bounds.maxX.resize(s);
        bounds.maxY.resize(s);
        for(size_t i = 0; i < lines->size(); ++i) {
          const ShapeRegistry::Raycast& line = lines->at(i);
          bounds.minX[i] = std::min(line.start.x, line.end.x);
          bounds.maxX[i] = std::max(line.start.x, line.end.x);
          bounds.minY[i] = std::min(line.start.y, line.end.y);
          bounds.maxY[i] = std::max(line.start.y, line.end.y);
        }
      });

      builder.submitTask(std::move(task));
    }
  };

  std::unique_ptr<ShapeRegistry::IShapeImpl> createIndividualLine() {
    return std::make_unique<IndividualLineShape>();
  }
}
