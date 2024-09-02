#pragma once

#include "glm/vec2.hpp"
#include "TableOperations.h"
#include "Physics.h"
#include "QueryAlias.h"

class ITableResolver;
class RuntimeDatabaseTaskBuilder;
struct PhysicsAliases;
class ElementRef;
class ElementRefResolver;

namespace pt {
  //TODO: should scale factor in to this?
  struct Transform {
    glm::vec2 transformPoint(const glm::vec2& p) const;

    glm::vec2 pos{};
    glm::vec2 rot{};
  };
  struct TransformAlias {
    TransformAlias(const PhysicsAliases& tables);

    using Q = QueryAlias<const Row<float>>;
    Q posX, posY, rotX, rotY;
  };
  struct TransformResolver {
    TransformResolver(RuntimeDatabaseTaskBuilder& task, const TransformAlias& a);

    Transform resolve(const ElementRef& ref);

    using FRow = CachedRow<const Row<float>>;
    FRow posX, posY, rotX, rotY;
    std::shared_ptr<ITableResolver> resolver;
    ElementRefResolver res;
    TransformAlias alias;
  };
}