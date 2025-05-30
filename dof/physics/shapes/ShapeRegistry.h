#pragma once

#include "Table.h"
#include "glm/vec2.hpp"
#include <variant>
#include "QueryAlias.h"
#include "DatabaseID.h"
#include <TransformResolver.h>
#include <Geometric.h>

struct UnpackedDatabaseElementID;
class IAppBuilder;
struct PhysicsAliases;
class RuntimeDatabaseTaskBuilder;
class ITableResolver;

namespace ShapeRegistry {
  struct Rectangle {
    glm::vec2 center{};
    glm::vec2 right{ 1, 0 };
    glm::vec2 halfWidth{ 0.5f };
  };
  struct Raycast {
    glm::vec2 start{};
    glm::vec2 end{};
  };
  struct AABB {
    glm::vec2 min{};
    glm::vec2 max{};
  };
  struct Circle {
    glm::vec2 pos{};
    float radius{};
  };
  struct Mesh {
    const std::vector<glm::vec2>& points;
    Geo::AABB aabb;
    pt::FullTransform transform;
  };
  using Variant = std::variant<std::monostate, Rectangle, Raycast, AABB, Circle, Mesh>;

  struct BodyType {
    Variant shape;
  };

  struct IShapeClassifier {
    virtual ~IShapeClassifier() = default;
    virtual BodyType classifyShape(const UnpackedDatabaseElementID& id) = 0;
  };

  //The center that "centerToContact" in the manifold is relative to
  glm::vec2 getCenter(const BodyType& shape);

  struct BroadphaseBounds {
    QueryAliasBase requiredDependency;
    TableID table;
    std::vector<float> minX, minY, maxX, maxY;
  };

  //For use by IShapeRegistry, tasks would generally use these indirectly via IShapeRegistry
  struct IShapeImpl {
    virtual ~IShapeImpl() = default;
    //Get all tables that can hold this shape impl
    virtual std::vector<TableID> queryTables(IAppBuilder& builder) const = 0;
    //Resolver to be invoked only on tables specified by queryTables
    virtual std::shared_ptr<IShapeClassifier> createShapeClassifier(RuntimeDatabaseTaskBuilder& task, ITableResolver& resolver) const = 0;
    //Submit a task that writes the bounds for the given table to the container
    //Must take a const dependency on the requiredDependency field for this to schedule insertion into broadphase properly
    virtual void writeBoundaries(IAppBuilder& builder, BroadphaseBounds& bounds) const = 0;
  };

  struct ShapeLookup {
    TableID table;
    const IShapeImpl* impl{};
  };

  //This is the glue between the implementation of shapes and their use cases
  //Ways to represent shapes are registered here
  //This then exposes operations that tasks can use to operate on any shape without needing to know
  //how to resolve the shapes.
  //It provides a bit of extra freedom in how shapes can be represented without forgetting to update their use locations.
  struct IShapeRegistry {
    virtual ~IShapeRegistry() = default;
    virtual void registerImpl(std::unique_ptr<IShapeImpl> impl) = 0;
    virtual void initLookupTable(IAppBuilder& builder) = 0;
    //Use the registered shapes to create a resolver that can resolve any shape type
    virtual std::shared_ptr<IShapeClassifier> createShapeClassifier(RuntimeDatabaseTaskBuilder& task) const = 0;
    virtual const std::vector<ShapeLookup>& lookup() const = 0;
  };

  struct ShapeRegistryStorage {
    ShapeRegistryStorage();
    ~ShapeRegistryStorage();
    std::unique_ptr<IShapeRegistry> registry;
  };

  struct GlobalRow : SharedRow<ShapeRegistryStorage> {};

  const IShapeRegistry* get(RuntimeDatabaseTaskBuilder& task);
  IShapeRegistry* getMutable(RuntimeDatabaseTaskBuilder& task);

  //Called after all shapes are registered during initialization
  void finalizeRegisteredShapes(IAppBuilder& builder);
};