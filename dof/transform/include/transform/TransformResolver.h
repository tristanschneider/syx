#pragma once

#include <AppBuilder.h>
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include <Table.h>
#include <Transform/TransformRows.h>

class ITableResolver;
class RuntimeDatabaseTaskBuilder;
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

    constexpr ResolveOps addWrite() const {
      auto result = *this;
      return result.writable = true, result;
    }

    bool resolveWorld{ true };
    bool resolveWorldInverse{ false };
    bool forceUpToDate{ false };
    bool writable{ false };
  };

  class Resolver {
  public:
    Resolver() = default;
    Resolver(RuntimeDatabaseTaskBuilder& task, const ResolveOps& ops = {});

    PackedTransform resolve(const ElementRef& ref);
    PackedTransform resolve(const UnpackedDatabaseElementID& ref);
    TransformPair resolvePair(const UnpackedDatabaseElementID& ref);
    TransformPair forceResolvePair(const UnpackedDatabaseElementID& ref);
    void write(const ElementRef& ref, const PackedTransform& value);

  private:
    ResolveOps ops;
    CachedRow<const WorldTransformRow> world;
    CachedRow<const WorldInverseTransformRow> worldInverse;
    CachedRow<const TransformNeedsUpdateRow> needsUpdate;
    std::shared_ptr<ITableResolver> resolver;
    ElementRefResolver res;
  };
}