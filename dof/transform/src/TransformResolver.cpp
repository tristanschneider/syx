
#include <transform/TransformResolver.h>

#include <AppBuilder.h>
#include <math/Geometric.h>

namespace Transform {
  Resolver::Resolver(RuntimeDatabaseTaskBuilder& task, const ResolveOps& o)
    : ops{ o }
    , resolver{ task.getResolver<>() }
    , res{ task.getRefResolver() }
  {
    if(o.resolveWorld) {
      task.logDependency({ QueryAlias<WorldTransformRow>::create().read() });
    }
    if(o.resolveWorldInverse) {
      task.logDependency({ QueryAlias<WorldInverseTransformRow>::create().read() });
    }
    if(o.forceUpToDate) {
      task.logDependency({ QueryAlias<TransformNeedsUpdateRow>::create().read() });
    }
  }

  PackedTransform Resolver::resolve(const ElementRef& ref) {
    auto raw = res.tryUnpack(ref);
    return raw ? resolve(*raw) : PackedTransform{};
  }

  PackedTransform Resolver::resolve(const UnpackedDatabaseElementID& ref) {
    assert(ops.resolveWorld);
    const PackedTransform* result = resolver->tryGetOrSwapRowElement(world, ref);
    return result ? *result : PackedTransform{};
  }

  TransformPair Resolver::resolvePair(const UnpackedDatabaseElementID& ref) {
    assert(ops.resolveWorld && ops.resolveWorldInverse);
    const size_t i = ref.getElementIndex();
    return resolver->tryGetOrSwapAllRows(ref, world, worldInverse)
      ? TransformPair{ world->at(i), worldInverse->at(i) }
      : TransformPair{};
  }

  TransformPair Resolver::forceResolvePair(const UnpackedDatabaseElementID& ref) {
    assert(ops.resolveWorld && ops.resolveWorldInverse && ops.forceUpToDate);
    const size_t i = ref.getElementIndex();
    if(resolver->tryGetOrSwapAllRows(ref, world, worldInverse, needsUpdate)) {
      const PackedTransform& w = world->at(i);
      if(needsUpdate->contains(i)) {
        return { w, w.inverse() };
      }
      return { w, worldInverse->at(i) };
    }
    return {};
  }
}