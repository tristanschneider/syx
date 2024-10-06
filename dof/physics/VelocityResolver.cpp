#include "Precompile.h"
#include "VelocityResolver.h"

#include "AppBuilder.h"
#include "QueryAlias.h"
#include "Physics.h"

#include "Geometric.h"

namespace pt {
  bool Velocities::lessThan(float epsilon) const {
    return glm::length(linear) + angular <= epsilon;
  }

  template<class T>
  VelocitiesAlias<T> createAlias(const PhysicsAliases& tables) {
    return VelocitiesAlias<T> {
      .qLinearX{ tables.linVelX },
      .qLinearY{ tables.linVelY },
      .qLinearZ{ tables.linVelZ },
      .qAngular{ tables.angVel }
    };
  }

  template<> static VelocitiesAlias<const Row<float>> VelocitiesAlias<const Row<float>>::create(const PhysicsAliases& tables) {
    return createAlias<const Row<float>>(tables);
  }
  template<> static VelocitiesAlias<Row<float>> VelocitiesAlias<Row<float>>::create(const PhysicsAliases& tables) {
    return createAlias<Row<float>>(tables);
  }

  std::shared_ptr<ITableResolver> getResolver(RuntimeDatabaseTaskBuilder& task, const VelocitiesVariant& tables) {
    return std::visit([&task](const auto& a) {
      return task.getAliasResolver(
        a.qLinearX,
        a.qLinearY,
        a.qLinearZ,
        a.qAngular
      );
    }, tables);
  }

  VelocityResolver::VelocityResolver(RuntimeDatabaseTaskBuilder& task, const VelocitiesVariant& tables)
    : resolver{ getResolver(task, tables) }
    , res{ task.getIDResolver()->getRefResolver() }
    , alias{ tables }
  {
  }

  Velocities VelocityResolver::resolve(const ElementRef& ref) {
    if(auto raw = res.tryUnpack(ref)) {
      return std::visit([&raw, this](auto& alias) {
        const size_t i = raw->getElementIndex();
        if(resolver->tryGetOrSwapRowAlias(alias.qLinearX, alias.rLinearX, *raw) &&
          resolver->tryGetOrSwapRowAlias(alias.qLinearY, alias.rLinearY, *raw) &&
          resolver->tryGetOrSwapRowAlias(alias.qAngular, alias.rAngular, *raw)) {
          float z{};
          if(resolver->tryGetOrSwapRowAlias(alias.qLinearZ, alias.rLinearZ, *raw)) {
            z = alias.rLinearZ->at(i);
          }
          return Velocities{
            .linear{ alias.rLinearX->at(i), alias.rLinearY->at(i) },
            .linearZ{ z },
            .angular{ alias.rAngular->at(i) }
          };
        }
        return Velocities{};
      }, alias);
    }
    return {};
  }

  void VelocityResolver::writeBack(const Velocities& v, const ElementRef& ref) {
    if(auto raw = res.tryUnpack(ref)) {
      auto& a = std::get<MutableVelocities>(alias);
      const size_t i = raw->getElementIndex();
      a.rLinearX->at(i) = v.linear.x;
      a.rLinearY->at(i) = v.linear.y;
      if(a.rLinearZ) {
        a.rLinearZ->at(i) = v.linearZ;
      }
      a.rAngular->at(i) = v.angular;
    }
  }
}