#pragma once

#include "glm/vec2.hpp"

class IAppBuilder;
struct TaskRange;
struct GameDB;
struct PhysicsTableIds;
struct DBEvents;
struct PhysicsAliases;
class RuntimeDatabaseTaskBuilder;
struct UnpackedDatabaseElementID;
struct StableElementID;
struct ResolvedIDs;
class ElementRef;

namespace SweepNPruneBroadphase {
  struct BoundariesConfig;
}

namespace ShapeRegistry {
  struct IShapeClassifier;
}
namespace Shapes {
  struct RectDefinition;
}

class IPhysicsBodyResolver {
public:
  using Key = ResolvedIDs;

  virtual ~IPhysicsBodyResolver() = default;
  virtual std::optional<Key> tryResolve(const StableElementID& e) = 0;
  virtual std::optional<Key> tryResolve(const ElementRef& e) = 0;
  virtual glm::vec2 getCenter(const Key& e) = 0;
  virtual glm::vec2 getLinearVelocity(const Key& e) = 0;
  virtual float getAngularVelocity(const Key& e) = 0;
};

namespace PhysicsSimulation {
  Shapes::RectDefinition getRectDefinition();
  PhysicsAliases getPhysicsAliases();

  std::shared_ptr<ShapeRegistry::IShapeClassifier> createShapeClassifier(RuntimeDatabaseTaskBuilder& task);
  std::shared_ptr<IPhysicsBodyResolver> createPhysicsBodyResolver(RuntimeDatabaseTaskBuilder& task);

  void init(IAppBuilder& builder);
  void initFromConfig(IAppBuilder& builder);
  //Insert all objects into the broadphase that can be. Doesn't check if they already are, meant for initial insert
  void updatePhysics(IAppBuilder& builder);
  //Before table service
  void preProcessEvents(IAppBuilder& builder);
  //After table service
  void postProcessEvents(IAppBuilder& builder);
};