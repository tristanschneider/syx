#include "Precompile.h"
#include "shapes/Rectangle.h"
#include "AppBuilder.h"

namespace TableExt {
  inline glm::vec2 read(size_t i, const Row<float>& a, const Row<float>& b) {
    return { a.at(i), b.at(i) };
  }
};

namespace Shapes {
  struct IndividualRectShape : ShapeRegistry::IShapeImpl {
    std::vector<UnpackedDatabaseElementID> queryTables(IAppBuilder& builder) const final {
      return builder.queryTables<const RectangleRow>().matchingTableIDs;
    }

    struct Classifier : ShapeRegistry::IShapeClassifier {
      Classifier(RuntimeDatabaseTaskBuilder& task, ITableResolver& res)
        : resolver{ res }
      {
        //Log the dependency with get, but use the shared resolver
        task.getResolver(row);
      }

      ShapeRegistry::BodyType classifyShape(const UnpackedDatabaseElementID& id) final {
        if(const ShapeRegistry::Rectangle* rect = resolver.tryGetOrSwapRowElement(row, id)) {
          return { { *rect } };
        }
        return {};
      }

      ITableResolver& resolver;
      CachedRow<const RectangleRow> row;
    };

    std::shared_ptr<ShapeRegistry::IShapeClassifier> createShapeClassifier(RuntimeDatabaseTaskBuilder& task, ITableResolver& resolver) const final {
      return std::make_shared<Classifier>(task, resolver);
    }
  };

  struct SharedRectShape : ShapeRegistry::IShapeImpl {
    SharedRectShape(const RectDefinition& rectDef)
      : rect{ rectDef }
    {}

    std::vector<UnpackedDatabaseElementID> queryTables(IAppBuilder& builder) const final {
      return builder.queryTables<const SharedRectangleRow>().matchingTableIDs;
    }

    struct Classifier : ShapeRegistry::IShapeClassifier {
      Classifier(RuntimeDatabaseTaskBuilder& task, ITableResolver& res, const RectDefinition& rectDef)
        : resolver{ res }
        , rect{ rectDef }
      {
        //Log the dependency with get, but use the shared resolver
        task.getAliasResolver(
          rect.centerX, rect.centerY,
          rect.rotX, rect.rotY,
          rect.scaleX, rect.scaleY
        );
      }

      ShapeRegistry::BodyType classifyShape(const UnpackedDatabaseElementID& id) final {
        size_t myIndex = id.getElementIndex();
        if(resolver.tryGetOrSwapRowAlias(rect.centerX, centerX, id) &&
          resolver.tryGetOrSwapRowAlias(rect.centerY, centerY, id)
        ) {
          ShapeRegistry::Rectangle result;
          result.center = TableExt::read(myIndex, *centerX, *centerY);
          //Rotation is optional
          if(resolver.tryGetOrSwapRowAlias(rect.rotX, rotX, id) &&
            resolver.tryGetOrSwapRowAlias(rect.rotY, rotY, id)
          ) {
            result.right = TableExt::read(myIndex, *rotX, *rotY);
          }
          if(resolver.tryGetOrSwapRowAlias(rect.scaleX, scaleX, id) &&
            resolver.tryGetOrSwapRowAlias(rect.scaleY, scaleY, id)) {
            result.halfWidth = TableExt::read(myIndex, *scaleX, *scaleY) * 0.5f;
          }
          return { ShapeRegistry::Variant{ result } };
        }
        return {};
      }

      const RectDefinition& rect;
      ITableResolver& resolver;
      CachedRow<const Row<float>> centerX;
      CachedRow<const Row<float>> centerY;
      CachedRow<const Row<float>> rotX;
      CachedRow<const Row<float>> rotY;
      CachedRow<const Row<float>> scaleX;
      CachedRow<const Row<float>> scaleY;
    };

    std::shared_ptr<ShapeRegistry::IShapeClassifier> createShapeClassifier(RuntimeDatabaseTaskBuilder& task, ITableResolver& resolver) const final {
      return std::make_shared<Classifier>(task, resolver, rect);
    }

    RectDefinition rect;
  };

  std::unique_ptr<ShapeRegistry::IShapeImpl> createIndividualRectangle() {
    return std::make_unique<IndividualRectShape>();
  }

  std::unique_ptr<ShapeRegistry::IShapeImpl> createSharedRectangle(const RectDefinition& rect) {
    return std::make_unique<SharedRectShape>(rect);
  }
}
