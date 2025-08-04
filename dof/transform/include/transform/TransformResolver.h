#pragma once

#include <AppBuilder.h>
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include <Table.h>
#include <Transform/TransformRows.h>

class ITableResolver;
class RuntimeDatabaseTaskBuilder;
struct PhysicsAliases;
class ElementRef;
class ElementRefResolver;

namespace Transform {
  struct TransformPair {
    PackedTransform modelToWorld, worldToModel;
  };

  struct ResolveOps {
    constexpr ResolveOps addInverse() const {
      auto result = *this;
      return result.resolveWorldInverse = true, result;
    }

    constexpr ResolveOps addForceUpdate() const {
      auto result = *this;
      return result.forceUpToDate = true, result;
    }

    bool resolveWorld{ true };
    bool resolveWorldInverse{ false };
    bool forceUpToDate{ false };
  };

  class Resolver {
  public:
    Resolver() = default;
    Resolver(RuntimeDatabaseTaskBuilder& task, const ResolveOps& ops = {});

    PackedTransform resolve(const ElementRef& ref);
    PackedTransform resolve(const UnpackedDatabaseElementID& ref);
    TransformPair resolvePair(const UnpackedDatabaseElementID& ref);
    TransformPair forceResolvePair(const UnpackedDatabaseElementID& ref);

  private:
    ResolveOps ops;
    CachedRow<const WorldTransformRow> world;
    CachedRow<const WorldInverseTransformRow> worldInverse;
    CachedRow<const TransformNeedsUpdateRow> needsUpdate;
    std::shared_ptr<ITableResolver> resolver;
    ElementRefResolver res;
  };
}