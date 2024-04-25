#include "Precompile.h"
#include "shapes/Circle.h"
#include "AppBuilder.h"

namespace Shapes {
  struct IndividualCircleShape : ShapeRegistry::IShapeImpl {
    std::vector<UnpackedDatabaseElementID> queryTables(IAppBuilder& builder) const final {
      return builder.queryTables<const CircleRow>().matchingTableIDs;
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
  };

  std::unique_ptr<ShapeRegistry::IShapeImpl> createIndividualCircle() {
    return std::make_unique<IndividualCircleShape>();
  }
}
