#pragma once

#include "Profile.h"
#include "glm/vec2.hpp"
#include "config/Config.h"
#include "Scheduler.h"
#include "Table.h"
#include "AppBuilder.h"

class IAppModule;

struct SpatialQueriesTableTag : SharedRow<char> {};

struct AccelX : Row<float> {};
struct AccelY : Row<float> {};
struct AccelZ : Row<float> {};
struct VelX : Row<float> {};
struct VelY : Row<float> {};
//Optional, without this bodies are considered to be at z=0 and immobile along z
struct VelZ : Row<float> {};
struct VelA : Row<float> {};

namespace ShapeRegistry {
  struct IShapeClassifier;
}

namespace Physics {
  constexpr float DEFAULT_Z = 0;

  //TODO: expose createDatabase here rather than having gameplay create the physics tables

  std::unique_ptr<IAppModule> createModule(std::function<size_t(RuntimeDatabaseTaskBuilder&)> threadCount);

  void integrateVelocity(IAppBuilder& builder);
  void integratePositionAndRotation(IAppBuilder& builder);
  void applyDampingMultiplier(IAppBuilder& builder, const float& linearMultiplier, const float& angularMultiplier);
  std::shared_ptr<ShapeRegistry::IShapeClassifier> createShapeClassifier(RuntimeDatabaseTaskBuilder& task);
};