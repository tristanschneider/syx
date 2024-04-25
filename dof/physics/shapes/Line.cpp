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
  };

  std::unique_ptr<ShapeRegistry::IShapeImpl> createIndividualLine() {
    return std::make_unique<IndividualLineShape>();
  }
}
