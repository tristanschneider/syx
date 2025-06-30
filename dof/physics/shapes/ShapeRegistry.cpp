#include "Precompile.h"
#include "shapes/ShapeRegistry.h"
#include "generics/HashMap.h"

#include "AppBuilder.h"

namespace ShapeRegistry {
  const IShapeRegistry* get(RuntimeDatabaseTaskBuilder& task) {
    return task.query<const GlobalRow>().tryGetSingletonElement()->registry.get();
  }

  IShapeRegistry* getMutable(RuntimeDatabaseTaskBuilder& task) {
    return task.query<GlobalRow>().tryGetSingletonElement()->registry.get();
  }

  //Not the actual center, more like the reference point
  struct CenterVisitor {
    glm::vec2 operator()(const Rectangle& v) const {
      return v.center;
    }
    glm::vec2 operator()(const Raycast& v) const {
      return v.start;
    }
    glm::vec2 operator()(const AABB& v) const {
      return v.min;
    }
    glm::vec2 operator()(const Circle& v) const {
      return v.pos;
    }
    glm::vec2 operator()(const std::monostate&) const {
      return { 0, 0 };
    }
    glm::vec2 operator()(const Mesh& m) const {
      return m.modelToWorld.transformPoint(m.aabb.center());
    }
  };

  glm::vec2 getCenter(const BodyType& shape) {
    return std::visit(CenterVisitor{}, shape.shape);
  }

  struct ShapeRegistryImpl : IShapeRegistry {
    struct ShapeImpl {
      std::unique_ptr<IShapeImpl> impl;
    };
    struct ShapeOperations {
      const IShapeImpl* impl{};
    };

    struct ClassifierCollection : IShapeClassifier {
      BodyType classifyShape(const UnpackedDatabaseElementID& id) final {
        if(const size_t ti = id.getTableIndex(); ti < lookup.size() && lookup[ti]) {
          return lookup[ti]->classifyShape(id);
        }
        return {};
      }

      //Table index to classifier for that table
      std::vector<std::shared_ptr<IShapeClassifier>> lookup;
      //Hack to share a common resolver which is a bit more efficient than each impl creating their own as they'll all need one
      std::shared_ptr<ITableResolver> resolver;
    };

    void registerImpl(std::unique_ptr<IShapeImpl> impl) final {
      shapes.push_back({ std::move(impl) });
    }

    void initLookupTable(IAppBuilder& builder) final {
      for(ShapeImpl& shape : shapes) {
        for(const UnpackedDatabaseElementID& table : shape.impl->queryTables(builder)) {
          const size_t ti = table.getTableIndex();
          if(operations.size() <= ti) {
            operations.resize(ti + 1);
          }
          ShapeOperations& op = operations[ti];
          assert(op.impl == nullptr && "Tables must only define a single type of shape");
          op.impl = shape.impl.get();
          lookupStorage.push_back({ table, op.impl });
        }
      }
    }

    //Use the registered shapes to create a resolver that can resolve any shape type
    std::shared_ptr<IShapeClassifier> createShapeClassifier(RuntimeDatabaseTaskBuilder& task) const final {
      auto result = std::make_shared<ClassifierCollection>();
      //Fake dependency, actual dependency logged by the impls when they create their classifiers
      result->resolver = task.getResolver<const Row<std::monostate>>();
      std::transform(operations.begin(), operations.end(), std::back_inserter(result->lookup), [&task, &result](const ShapeOperations& op) {
        return op.impl ? op.impl->createShapeClassifier(task, *result->resolver) : std::shared_ptr<IShapeClassifier>{};
      });
      return result;
    }

    const std::vector<ShapeLookup>& lookup() const final {
      return lookupStorage;
    }

    std::vector<ShapeLookup> lookupStorage;
    std::vector<ShapeImpl> shapes;
    std::vector<ShapeOperations> operations;
  };

  //Called after all shapes are registered during initialization
  void finalizeRegisteredShapes(IAppBuilder& builder) {
    auto temp = builder.createTask();
    temp.discard();
    getMutable(temp)->initLookupTable(builder);
  }

  ShapeRegistryStorage::ShapeRegistryStorage()
    : registry{ std::make_unique<ShapeRegistryImpl>() }
  {
  }
  ShapeRegistryStorage::~ShapeRegistryStorage() = default;
}