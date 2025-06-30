#pragma once

#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "Physics.h"
#include "QueryAlias.h"
#include "Transform.h"

class ITableResolver;
class RuntimeDatabaseTaskBuilder;
struct PhysicsAliases;
class ElementRef;
class ElementRefResolver;

namespace pt {
  //Intended for light use or random access of transform data. For heavier uses manual queries may be more applicable
  struct Transform {
    glm::vec2 transformPoint(const glm::vec2& p) const;
    glm::vec2 transformVector(const glm::vec2& v) const;
    glm::vec2 inverseTransformPoint(const glm::vec2& p) const;
    glm::vec2 inverseTransformVector(const glm::vec2& v) const;

    glm::vec2 pos{};
    glm::vec2 rot{};
  };

  struct FullTransform {
    glm::vec3 transformPoint(const glm::vec3& p) const;
    glm::vec3 transformVector(const glm::vec3& v) const;
    glm::vec3 inverseTransformPoint(const glm::vec3& p) const;
    glm::vec3 inverseTransformVector(const glm::vec3& v) const;

    Parts toParts() const {
      return Parts{ .rot = rot, .scale = scale, .translate = pos };
    }

    PackedTransform toPacked() const {
      return PackedTransform::build(toParts());
    }

    std::pair<PackedTransform, PackedTransform> toPackedWithInverse() const {
      const auto parts = toParts();
      return std::make_pair(PackedTransform::build(parts), PackedTransform::inverse(parts));
    }

    glm::vec3 pos{};
    glm::vec2 rot{};
    glm::vec2 scale{};
  };

  struct TransformAlias {
    TransformAlias() = default;
    TransformAlias(const PhysicsAliases& tables);

    using Q = QueryAlias<const Row<float>>;
    Q posX, posY, rotX, rotY;
  };

  struct FullTransformAlias : TransformAlias {
    FullTransformAlias() = default;
    FullTransformAlias(const PhysicsAliases& tables);

    Q posZ, scaleX, scaleY;
  };

  struct TransformResolver {
    TransformResolver() = default;
    TransformResolver(RuntimeDatabaseTaskBuilder& task, const TransformAlias& a);

    Transform resolve(const ElementRef& ref);

    using FRow = CachedRow<const Row<float>>;
    FRow posX, posY, rotX, rotY;
    std::shared_ptr<ITableResolver> resolver;
    ElementRefResolver res;
    TransformAlias alias;
  };

  struct FullTransformResolver {
    FullTransformResolver() = default;
    FullTransformResolver(RuntimeDatabaseTaskBuilder& task, const FullTransformAlias& a);

    FullTransform resolve(const ElementRef& ref);
    FullTransform resolve(const UnpackedDatabaseElementID& ref);

    using FRow = CachedRow<const Row<float>>;
    FRow posX, posY, posZ, rotX, rotY, scaleX, scaleY;
    std::shared_ptr<ITableResolver> resolver;
    ElementRefResolver res;
    FullTransformAlias alias;
  };
}