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

  glm::vec2 Transform::inverseTransformPoint(const glm::vec2& p) const {
    return Geo::rotate(Geo::transposeRot(rot), p - pos);
  }

  glm::vec2 Transform::inverseTransformVector(const glm::vec2& v) const {
    return Geo::rotate(Geo::transposeRot(rot), v);
  }

  glm::vec3 FullTransform::transformPoint(const glm::vec3& p) const {
     const glm::vec2 res2 = Geo::rotate(rot, glm::vec2{ p.x, p.y } * scale);
     return pos + glm::vec3{ res2.x, res2.y, p.z };
  }

  glm::vec3 FullTransform::transformVector(const glm::vec3& v) const {
     const glm::vec2 res2 = Geo::rotate(rot, glm::vec2{ v.x, v.y } * scale);
     return { res2.x, res2.y, v.z };
  }

  glm::vec3 FullTransform::inverseTransformPoint(const glm::vec3& p) const {
    const glm::vec2 res2 = Geo::inverseOrZero(scale) * Geo::rotate(Geo::transposeRot(rot), glm::vec2{ p.x, p.y } - glm::vec2{ pos.x, pos.y });
    return { res2.x, res2.y, p.z - pos.z };
  }

  glm::vec3 FullTransform::inverseTransformVector(const glm::vec3& v) const {
    const glm::vec2 res2 = Geo::inverseOrZero(scale) * Geo::rotate(Geo::transposeRot(rot), glm::vec2{ v.x, v.y });
    return { res2.x, res2.y, v.z };
  }

  TransformAlias::TransformAlias(const PhysicsAliases& tables)
    : posX{ tables.posX.read() }
    , posY{ tables.posY.read() }
    , rotX{ tables.rotX.read() }
    , rotY{ tables.rotY.read() }
  {
  }

  FullTransformAlias::FullTransformAlias(const PhysicsAliases& tables)
    : TransformAlias{ tables }
    , posZ{ tables.posZ.read() }
    , scaleX{ tables.scaleX.read() }
    , scaleY{ tables.scaleY.read() }
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

  FullTransformResolver::FullTransformResolver(RuntimeDatabaseTaskBuilder& task, const FullTransformAlias& tables)
    : resolver{
      task.getAliasResolver(
        tables.posX.read(),
        tables.posY.read(),
        tables.posZ.read(),
        tables.rotX.read(),
        tables.rotY.read(),
        tables.scaleX.read(),
        tables.scaleY.read(),
        QueryAlias<TransformRow>::create().read()
      )
    }
    , res{ task.getIDResolver()->getRefResolver() }
    , alias{ tables }
  {
  }

  FullTransform FullTransformResolver::resolve(const ElementRef& ref) {
    auto raw = res.tryUnpack(ref);
    return raw ? resolve(*raw) : FullTransform{};
  }

  FullTransform FullTransformResolver::resolve(const UnpackedDatabaseElementID& ref) {
    const size_t i = ref.getElementIndex();
    if(resolver->tryGetOrSwapRowAlias(alias.posX, posX, ref) &&
      resolver->tryGetOrSwapRowAlias(alias.posY, posY, ref) &&
      resolver->tryGetOrSwapRowAlias(alias.rotX, rotX, ref) &&
      resolver->tryGetOrSwapRowAlias(alias.rotY, rotY, ref)
    ) {
      FullTransform result;
      if(resolver->tryGetOrSwapRowAlias(alias.posZ, posZ, ref)) {
        result.pos.z = posZ->at(i);
      }
      if(resolver->tryGetOrSwapRowAlias(alias.scaleX, scaleX, ref) &&
        resolver->tryGetOrSwapRowAlias(alias.scaleY, scaleY, ref)
      ) {
        result.scale = { scaleX->at(i), scaleY->at(i) };
      }
      else {
        result.scale = { 1, 1 };
      }

      result.pos.x = posX->at(i);
      result.pos.y = posY->at(i);
      result.rot = { rotX->at(i), rotY->at(i) };

      return result;
    }
    return {};
  }

  TransformPair FullTransformResolver::resolvePair(const UnpackedDatabaseElementID& ref) {
    const pt::TransformPair* result = resolver->tryGetOrSwapRowElement(transformRow, ref);
    return result ? *result : resolve(ref).toPackedWithInverse();
  }
}