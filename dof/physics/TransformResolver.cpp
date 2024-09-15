#include "Precompile.h"
#include "TransformResolver.h"

#include "AppBuilder.h"
#include "QueryAlias.h"
#include "Physics.h"

#include "Geometric.h"

namespace pt {
  glm::vec2 Transform::transformPoint(const glm::vec2& p) const {
    return pos + Geo::rotate(rot, p);
  }

  glm::vec2 Transform::transformVector(const glm::vec2& v) const {
    return Geo::rotate(rot, v);
  }

  TransformAlias::TransformAlias(const PhysicsAliases& tables)
    : posX{ tables.posX.read() }
    , posY{ tables.posY.read() }
    , rotX{ tables.rotX.read() }
    , rotY{ tables.rotY.read() }
  {
  }

  TransformResolver::TransformResolver(RuntimeDatabaseTaskBuilder& task, const TransformAlias& tables)
    : resolver{
      task.getAliasResolver(
        tables.posX.read(),
        tables.posY.read(),
        tables.rotX.read(),
        tables.rotY.read()
      )
    }
    , res{ task.getIDResolver()->getRefResolver() }
    , alias{ tables }
  {
  }

  Transform TransformResolver::resolve(const ElementRef& ref) {
    if(auto raw = res.tryUnpack(ref)) {
      const size_t i = raw->getElementIndex();
      if(resolver->tryGetOrSwapRowAlias(alias.posX, posX, *raw) &&
        resolver->tryGetOrSwapRowAlias(alias.posY, posY, *raw) &&
        resolver->tryGetOrSwapRowAlias(alias.rotX, rotX, *raw) &&
        resolver->tryGetOrSwapRowAlias(alias.rotY, rotY, *raw)) {
        return {
          .pos{ posX->at(i), posY->at(i) },
          .rot{ rotX->at(i), rotY->at(i) }
        };
      }
    }
    return {};
  }

}