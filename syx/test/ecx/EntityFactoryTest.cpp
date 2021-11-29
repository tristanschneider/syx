#include "Precompile.h"
#include "CppUnitTest.h"

#include "EntityFactory.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ecx {
  TEST_CLASS(EntityFactoryTest) {
    using TestEntity = uint32_t;
    using TestEntityRegistry = EntityRegistry<TestEntity>;
    using TestEntityFactory = EntityFactory<TestEntity>;

    TEST_METHOD(EntityFactory_CreateEntity_IsCreated) {
      TestEntityRegistry registry;
      EntityFactory factory(registry);

      auto entity = factory.createEntity();
      registry.addComponent<int>(entity, 5);

      Assert::AreEqual(size_t(1), registry.size<int>());
    }

    TEST_METHOD(EntityFactory_DestroyEntity_IsDestroyed) {
      TestEntityRegistry registry;
      auto entity = registry.createEntity();
      registry.addComponent<int>(entity, 99);
      EntityFactory factory(registry);

      factory.destroyEntity(entity);

      Assert::AreEqual(size_t(0), registry.size<int>());
    }
  };
}