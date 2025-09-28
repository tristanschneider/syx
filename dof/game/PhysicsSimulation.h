#pragma once

#include "glm/vec2.hpp"

class IAppBuilder;
struct TaskRange;
struct GameDB;
class RuntimeDatabaseTaskBuilder;
struct UnpackedDatabaseElementID;
class ElementRef;

class IPhysicsBodyResolver {
public:
  using Key = UnpackedDatabaseElementID;

  virtual ~IPhysicsBodyResolver() = default;
  virtual std::optional<Key> tryResolve(const ElementRef& e) = 0;
  virtual glm::vec2 getCenter(const Key& e) = 0;
  virtual glm::vec2 getLinearVelocity(const Key& e) = 0;
  virtual float getAngularVelocity(const Key& e) = 0;
};

namespace PhysicsSimulation {
  std::shared_ptr<IPhysicsBodyResolver> createPhysicsBodyResolver(RuntimeDatabaseTaskBuilder& task);

  //Insert all objects into the broadphase that can be. Doesn't check if they already are, meant for initial insert
  void updatePhysics(IAppBuilder& builder);
};