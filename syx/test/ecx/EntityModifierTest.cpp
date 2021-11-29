#include "Precompile.h"
#include "CppUnitTest.h"

#include "EntityModifier.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ecx {
  TEST_CLASS(EntityModifierTest) {
    using TestEntity = uint32_t;
    using TestEntityRegistry = EntityRegistry<TestEntity>;
    template<class... Components>
    using TestEntityModifier = EntityModifier<TestEntity, Components...>;

    TEST_METHOD(EntityModifier_AddComponent_IsAdded) {
      TestEntityRegistry registry;
      TestEntityModifier<int> modifier(registry);
      auto entity = registry.createEntity();

      modifier.addComponent<int>(entity, 10);

      const int* created = registry.tryGetComponent<int>(entity);
      Assert::IsNotNull(created);
      Assert::AreEqual(10, *created);
    }

    TEST_METHOD(EntityModifier_GetOrAddNotExisting_IsCreated) {
      TestEntityRegistry registry;
      TestEntityModifier<int> modifier(registry);
      auto entity = registry.createEntity();

      modifier.getOrAddComponent<int>(entity);

      const int* created = registry.tryGetComponent<int>(entity);
      Assert::IsNotNull(created);
    }

    TEST_METHOD(EntityModifier_GetOrAddExisting_IsReturned) {
      TestEntityRegistry registry;
      TestEntityModifier<int> modifier(registry);
      auto entity = registry.createEntity();
      registry.addComponent<int>(entity, 11);

      const int& fetched = modifier.getOrAddComponent<int>(entity);

      Assert::AreEqual(11, fetched);
    }

    TEST_METHOD(EntityModifier_RemoveComponent_IsRemoved) {
      TestEntityRegistry registry;
      TestEntityModifier<int> modifier(registry);
      auto entity = registry.createEntity();
      registry.addComponent<int>(entity, 3);

      modifier.removeComponent<int>(entity);

      Assert::AreEqual(size_t(0), registry.size<int>());
    }
  };
}