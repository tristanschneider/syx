#include "Precompile.h"
#include "CppUnitTest.h"

#include "EntityRegistry.h"
#include "View.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ecx {
  TEST_CLASS(EntityRegistryTest) {
    using TestEntity = uint32_t;
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

    TEST_METHOD(EntityRegistry_AddRemoveEmptyComponent_Compiles) {
      TestEntityRegistry registry;
      auto entity = registry.createEntity();
      struct EmptyComponent {};

      registry.addComponent<EmptyComponent>(entity);
      Assert::IsTrue(registry.hasComponent<EmptyComponent>(entity));
      registry.removeComponent<EmptyComponent>(entity);
    }

    TEST_METHOD(EntityRegistry_BeginEmpty_EqualsEnd) {
      TestEntityRegistry registry;

      Assert::IsTrue(registry.begin<int>() == registry.end<int>());
    }

    TEST_METHOD(EntityRegistry_BeginNoneOfType_EqualsEnd) {
      TestEntityRegistry registry;
      auto entity = registry.createEntity();
      registry.addComponent<int>(entity);

      Assert::IsTrue(registry.begin<short>() == registry.end<short>());
    }

    TEST_METHOD(EntityRegistry_BeginOne_IsEntity) {
      TestEntityRegistry registry;
      auto entity = registry.createEntity();
      registry.addComponent<int>(entity, 10);
      auto begin = registry.begin<int>();

      Assert::IsFalse(begin == registry.end<int>());
      Assert::AreEqual(entity, begin.entity());
      Assert::AreEqual(10, begin.component());
    }

    TEST_METHOD(EntityRegistry_IterateWithHoles_AllEntitiesFound) {
      TestEntityRegistry registry;
      registry.createEntity();
      auto b = registry.createEntity();
      auto c = registry.createEntity();
      auto d = registry.createEntity();
      registry.addComponent<int>(b, 3);
      registry.addComponent<char>(c, 'a');
      registry.addComponent<int>(d, 4);

      std::unordered_set<uint32_t> foundEntities;
      for(auto it = registry.begin<int>(); it != registry.end<int>(); ++it) {
        foundEntities.insert(it.entity());
      }

      Assert::IsTrue(foundEntities == std::unordered_set<uint32_t>({ b, d }));
    }

    TEST_METHOD(EntityRegistry_FindNonexistent_NotFound) {
      TestEntityRegistry registry;
      auto entity = registry.createEntity();

      auto it = registry.find<int>(++entity);

      Assert::IsTrue(it == registry.end<int>());
    }

    TEST_METHOD(EntityRegistry_FindNoComponent_NotFound) {
      TestEntityRegistry registry;
      auto entity = registry.createEntity();

      auto it = registry.find<int>(entity);

      Assert::IsTrue(it == registry.end<int>());
    }

    TEST_METHOD(EntityRegistry_FindWithComponent_Found) {
      TestEntityRegistry registry;
      auto entity = registry.createEntity();
      registry.addComponent<int>(entity, 7);

      auto it = registry.find<int>(entity);

      Assert::IsFalse(it == registry.end<int>());
      Assert::AreEqual(entity, it.entity());
      Assert::AreEqual(7, it.component());
    }

    TEST_METHOD(EntityRegistry_SingletonEntity_HasSingletonComponent) {
      TestEntityRegistry registry;

      Assert::IsTrue(registry.find<SingletonComponent>(registry.getSingleton()) != registry.end<SingletonComponent>());
    }

    TEST_METHOD(EntityRegistry_SingletonAddComponent_HasComponent) {
      TestEntityRegistry registry;
      auto entity = registry.getSingleton();
      registry.addComponent<int>(entity, 10);

      auto it = registry.find<int>(entity);

      Assert::IsFalse(it == registry.end<int>());
      Assert::AreEqual(it.component(), 10);
    }

    TEST_METHOD(EntityRegistry_PoolsForLoop_Ends) {
      TestEntityRegistry registry;
      int counter = 0;

      for(auto it = registry.poolsBegin(); it != registry.poolsEnd() && counter++ < 100; ++it) {
      }

      Assert::IsTrue(counter < 100);
    }

    static size_t getPoolCount(TestEntityRegistry& registry) {
      size_t count = 0;
      for(auto it = registry.poolsBegin(); it != registry.poolsEnd(); ++it) {
        ++count;
      }
      return count;
    }

    TEST_METHOD(EntityRegistry_IteratePoolAndView_NoNewPoolsCreated) {
      TestEntityRegistry registry;
      const size_t originalPoolCount = getPoolCount(registry);
      Assert::IsTrue(originalPoolCount > 0, L"This test assumes there's some default pool");

      for(auto it = registry.poolsBegin(); it != registry.poolsEnd(); ++it) {
        ecx::View<TestEntity, Read<int>> view(registry);
        for(auto&& thing : view) {
          (void)thing;
        }
      }

      const size_t newPoolCount = getPoolCount(registry);
      Assert::AreEqual(originalPoolCount, newPoolCount);
    }
  };
}