#pragma once

#include "glm/vec2.hpp"
#include "QueryAlias.h"
#include "AppBuilder.h"

class ITableResolver;
class RuntimeDatabaseTaskBuilder;
struct PhysicsAliases;
class ElementRef;
class ElementRefResolver;

//Convenience wrapper for accessing and potentially modifying all velocity information
//Likely not as efficient as manually querying the tables but simpler
namespace pt {
  struct Velocities {
    bool lessThan(float epsilon) const;

    glm::vec2 linear{};
    float linearZ{};
    float angular{};
  };
  //Junk to allow sharing code between const and mutable versions of the resolver
  //In either case the query aliases and rows are the same but either pointing at the const or mutable versions
  template<class QT>
  struct VelocitiesAlias {
    using Q = QueryAlias<QT>;

    static VelocitiesAlias<QT> create(const PhysicsAliases&);

    Q qLinearX, qLinearY, qLinearZ, qAngular;
    CachedRow<QT> rLinearX, rLinearY, rLinearZ, rAngular;
  };

  template<> static VelocitiesAlias<const Row<float>> VelocitiesAlias<const Row<float>>::create(const PhysicsAliases&);
  template<> static VelocitiesAlias<Row<float>> VelocitiesAlias<Row<float>>::create(const PhysicsAliases&);

  using ConstVelocities = VelocitiesAlias<const Row<float>>;
  using MutableVelocities = VelocitiesAlias<Row<float>>;
  using VelocitiesVariant = std::variant<ConstVelocities, MutableVelocities>;
  struct VelocityResolver {
    VelocityResolver(RuntimeDatabaseTaskBuilder& task, const VelocitiesVariant& a);

    //Can be used in const and mutable modes
    Velocities resolve(const ElementRef& ref);
    //Write back a value that was just fetched from resolve
    //Caller must make sure it was just fetched and that this was created with write access
    void writeBack(const Velocities& v, const ElementRef& ref);

    std::shared_ptr<ITableResolver> resolver;
    ElementRefResolver res;
    VelocitiesVariant alias;
  };
}