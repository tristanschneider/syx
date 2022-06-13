#include "Precompile.h"
#include "CppUnitTest.h"

#include "EntityModifier.h"
#include "LinearEntityRegistry.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ecx {
  TEST_CLASS(LinearEntityModifierTest) {
    using TestEntity = LinearEntity;
    struct TestEntityRegistry : public EntityRegistry<TestEntity> {
      TestEntity createEntity() {
        return EntityRegistry<TestEntity>::createEntity(*getDefaultEntityGenerator());
      }

      template<class... Args>
      auto createEntityWithComponents() {
        return EntityRegistry<TestEntity>::createEntityWithComponents<Args...>(*getDefaultEntityGenerator());
      }
    };

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

    TEST_METHOD(EntityModifierNewChunk_RemoveComponentFromAllEntities_IsRemoved) {
      TestEntityRegistry registry;
      registry.createEntityWithComponents<uint8_t, uint16_t, uint32_t>();
      registry.createEntityWithComponents<uint8_t, uint32_t>();
      registry.createEntityWithComponents<uint16_t, uint64_t>();
      TestEntityModifier<uint8_t, uint32_t> modifier(registry);

      modifier.removeComponentsFromAllEntities<uint8_t, uint32_t>();

      Assert::AreEqual(size_t(0), registry.size<uint8_t>());
      Assert::AreEqual(size_t(0), registry.size<uint32_t>());
    }

    TEST_METHOD(EntityModifierExistingChunk_RemoveComponentFromAllEntities_IsRemoved) {
      TestEntityRegistry registry;
      registry.createEntityWithComponents<uint8_t, uint16_t, uint32_t>();
      registry.createEntityWithComponents<uint8_t, uint16_t>();
      registry.createEntityWithComponents<uint16_t, uint64_t>();
      TestEntityModifier<uint8_t, uint32_t> modifier(registry);

      modifier.removeComponentsFromAllEntities<uint8_t, uint32_t>();

      Assert::AreEqual(size_t(0), registry.size<uint8_t>());
      Assert::AreEqual(size_t(0), registry.size<uint32_t>());
    }

  };
}