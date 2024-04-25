#include "Precompile.h"
#include "shapes/AABB.h"
#include "AppBuilder.h"

namespace Shapes {
  struct IndividualAABBShape : ShapeRegistry::IShapeImpl {
    std::vector<UnpackedDatabaseElementID> queryTables(IAppBuilder& builder) const final {
      return builder.queryTables<const AABBRow>().matchingTableIDs;
    }

    struct Classifier : ShapeRegistry::IShapeClassifier {
      Classifier(RuntimeDatabaseTaskBuilder& task, ITableResolver& res)
        : resolver{ res }
      {
        //Log the dependency with get, but use the shared resolver
        task.getResolver(row);
      }

      ShapeRegistry::BodyType classifyShape(const UnpackedDatabaseElementID& id) final {
        if(const ShapeRegistry::AABB* bb = resolver.tryGetOrSwapRowElement(row, id)) {
          return { { *bb } };
        }
        return {};
      }

      ITableResolver& resolver;
      CachedRow<const AABBRow> row;
    };

    std::shared_ptr<ShapeRegistry::IShapeClassifier> createShapeClassifier(RuntimeDatabaseTaskBuilder& task, ITableResolver& resolver) const final {
      return std::make_shared<Classifier>(task, resolver);
    }
  };

  std::unique_ptr<ShapeRegistry::IShapeImpl> createIndividualAABB() {
    return std::make_unique<IndividualAABBShape>();
  }
}
