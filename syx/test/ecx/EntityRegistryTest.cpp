#include "Precompile.h"
#include "CppUnitTest.h"

#include "EntityRegistry.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ecx {
  TEST_CLASS(EntityRegistryTest) {
    using TestEntityRegistry = EntityRegistry<uint32_t>;

    TEST_METHOD(EntityRegistry_CreateEntity_IsValid) {
      TestEntityRegistry registry;

      auto entity = registry.createEntity();

      Assert::IsTrue(registry.isValid(entity));
    }

    TEST_METHOD(EntityRegistry_DestroyEntity_IsNotValid) {
      TestEntityRegistry registry;
      auto entity = registry.createEntity();

      registry.destroyEntity(entity);

      Assert::IsFalse(registry.isValid(entity));
    }

    TEST_METHOD(EntityRegistry_AddComponent_HasComponent) {
      TestEntityRegistry registry;
      auto entity = registry.createEntity();

      registry.addComponent<int>(entity, 7);

      int* added = registry.tryGetComponent<int>(entity);
      Assert::IsTrue(registry.hasComponent<int>(entity));
      Assert::IsNotNull(added);
      Assert::AreEqual(7, *added);
      Assert::AreEqual(7, registry.getComponent<int>(entity));
    }

    TEST_METHOD(EntityRegistry_RemoveComponent_NoComponent) {
      TestEntityRegistry registry;
      auto entity = registry.createEntity();
      registry.addComponent<int>(entity, 99);

      registry.removeComponent<int>(entity);

      Assert::IsFalse(registry.hasComponent<int>(entity));
      Assert::IsTrue(registry.isValid(entity));
    }

    TEST_METHOD(EntityRegistry_DeleteEntityWithComponents_DoesntHaveComponents) {
      TestEntityRegistry registry;
      auto entity = registry.createEntity();
      registry.addComponent<short>(entity, true);

      registry.destroyEntity(entity);

      Assert::IsFalse(registry.isValid(entity));
      Assert::IsFalse(registry.hasComponent<short>(entity));
    }

    TEST_METHOD(EntityRegistry_AddComponentToOne_OthersDontHaveIt) {
      TestEntityRegistry registry;
      auto entityA = registry.createEntity();
      auto entityB = registry.createEntity();

      registry.addComponent<uint32_t>(entityB);

      Assert::IsTrue(registry.hasComponent<uint32_t>(entityB));
      Assert::IsFalse(registry.hasComponent<uint32_t>(entityA));
    }

    TEST_METHOD(EntityRegistry_TryGetConstComponent_HasComponent) {
      TestEntityRegistry registry;
      auto entity = registry.createEntity();
      registry.addComponent<short>(entity, false);

      Assert::IsTrue(registry.hasComponent<const short>(entity));
    }

    TEST_METHOD(EntityRegistry_TryGetConstRefComponent_HasComponent) {
      TestEntityRegistry registry;
      auto entity = registry.createEntity();
      registry.addComponent<short>(entity, false);

      Assert::IsTrue(registry.hasComponent<const short&>(entity));
    }

    //Testing swap remove behavior of component pools
    TEST_METHOD(EntityRegistry_TwoComponentsRemoveOne_OtherStillExists) {
      TestEntityRegistry registry;
      auto entityA = registry.createEntity();
      auto entityB = registry.createEntity();
      registry.addComponent<int>(entityA, 9);
      registry.addComponent<int>(entityB, 12);

      registry.removeComponent<int>(entityA);

      Assert::IsTrue(registry.hasComponent<int>(entityB));
      Assert::AreEqual(12, registry.getComponent<int>(entityB));
    }
  };
}