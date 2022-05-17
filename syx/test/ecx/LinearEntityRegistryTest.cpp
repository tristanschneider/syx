#include "Precompile.h"
#include "CppUnitTest.h"

#include "LinearEntityRegistry.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ecx {
  TEST_CLASS(LinearEntityRegistryTest) {
    using TestEntityRegistry = EntityRegistry<LinearEntity>;

    TEST_METHOD(LinearEntity_TwoTypes_NotSame) {
      const uint32_t i = LinearEntity::buildChunkId<int>();
      const uint32_t s = LinearEntity::buildChunkId<short>();

      Assert::AreNotEqual(i, s);
    }

    TEST_METHOD(LinearEntity_MultiTypeDifferentOrder_SameId) {
      uint32_t a = LinearEntity::buildChunkId<int>();
      a = LinearEntity::buildChunkId<short>(a);

      uint32_t b = LinearEntity::buildChunkId<short>();
      b = LinearEntity::buildChunkId<int>(b);

      Assert::AreEqual(a, b);
    }

    TEST_METHOD(LinearEntity_TypePermutations_SameIds) {
      std::vector<typeId_t<LinearEntity>> types {
        typeId<int, LinearEntity>(),
        typeId<short, LinearEntity>(),
        typeId<std::string, LinearEntity>(),
        typeId<uint32_t, LinearEntity>(),
        typeId<uint64_t, LinearEntity>(),
        typeId<uint8_t, LinearEntity>()
      };

      std::optional<uint32_t> prev;
      do {
        std::optional<uint32_t> result;
        for(auto&& type : types) {
          result = result ? LinearEntity::buildChunkId(*result, type) : LinearEntity::buildChunkId(type);
        }
        if(prev) {
          Assert::AreEqual(*prev, *result);
        }
        prev = result;
      }
      while (std::next_permutation(types.begin(), types.end()));
    }

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
      Assert::IsTrue(entity.mData.mParts.mEntityId == begin.entity().mData.mParts.mEntityId);
      Assert::AreEqual(10, begin.component());
    }

    TEST_METHOD(EntityRegistry_BeginOnEmptyChunk_StartsOnSecondChunk) {
      TestEntityRegistry registry;
      auto entity = registry.createEntity();
      registry.addComponent<int>(entity, 10);
      registry.addComponent<char>(entity, true);

      Assert::AreEqual(10, *registry.begin<int>());
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
        foundEntities.insert(it.entity().mData.mParts.mEntityId);
      }

      Assert::IsTrue(foundEntities == std::unordered_set<uint32_t>({ b.mData.mParts.mEntityId, d.mData.mParts.mEntityId }));
    }

    TEST_METHOD(EntityRegistry_FindNonexistent_NotFound) {
      TestEntityRegistry registry;
      auto entity = registry.createEntity();

      auto it = registry.find<int>(LinearEntity(99));

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
      Assert::IsTrue(entity.mData.mParts.mEntityId == it.entity().mData.mParts.mEntityId);
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

    TEST_METHOD(EntityRegistry_TryCreateEntityNew_IsCreated) {
      TestEntityRegistry registry;

      Assert::IsTrue(registry.tryCreateEntity(LinearEntity(10, 0)).has_value());
    }

    TEST_METHOD(EntityRegistry_TryCreateTaken_NotCreated) {
      TestEntityRegistry registry;
      auto first = registry.createEntity();

      Assert::IsFalse(registry.tryCreateEntity(first).has_value());
    }

    TEST_METHOD(EntityRegistry_CreateNewWithSlotTaken_NewSlotChosen) {
      TestEntityRegistry registry;
      //This is the first id that will attempt to be generated
      registry.tryCreateEntity(LinearEntity(1, 0));

      auto entity = registry.createEntity();

      Assert::AreNotEqual(uint32_t(1), entity.mData.mParts.mEntityId);
    }

    TEST_METHOD(EntityRegistry_CreateWithComponents_HasComponents) {
      TestEntityRegistry registry;
      auto entity = registry.createEntityWithComponents<int, std::string>();

      Assert::IsTrue(registry.hasComponent<int>(entity));
      Assert::IsTrue(registry.hasComponent<std::string>(entity));
    }

    TEST_METHOD(EntityRegistry_TryCreateWithComponents_HasComponents) {
      TestEntityRegistry registry;
      LinearEntity desired(100, 0);
      auto entity = registry.tryCreateEntityWithComponents<int, std::string>(desired);

      Assert::IsTrue(entity.has_value());
      Assert::IsTrue(*entity == desired);
      Assert::IsTrue(registry.hasComponent<int>(*entity));
      Assert::IsTrue(registry.hasComponent<std::string>(*entity));
    }

    TEST_METHOD(EntityRegistry_CreateAndDestroyEntity_IsInvalid) {
      TestEntityRegistry registry;
      LinearEntity entity = registry.createEntity();

      registry.destroyEntity(entity);

      Assert::IsFalse(registry.isValid(entity));
    }

    TEST_METHOD(EntityRegistry_CreateDestroyCreate_IDReused) {
      TestEntityRegistry registry;
      LinearEntity entity = registry.createEntity();

      registry.destroyEntity(entity);
      LinearEntity newEntity = registry.createEntity();

      Assert::AreEqual(entity.mData.mParts.mEntityId, newEntity.mData.mParts.mEntityId);
      Assert::IsTrue(entity != newEntity);
      Assert::IsFalse(registry.isValid(entity));
    }

    TEST_METHOD(EntityRegistry_TryCreateReUsedId_Fails) {
      TestEntityRegistry registry;
      LinearEntity original = registry.createEntity();
      registry.destroyEntity(original);
      registry.createEntity();

      Assert::IsFalse(registry.tryCreateEntity(original).has_value());
    }

    TEST_METHOD(EntityRegistry_AddExistingComponent_ExistingReturned) {
      TestEntityRegistry registry;
      auto&& [ entity, i ] = registry.createAndGetEntityWithComponents<int>();
      i.get() = 5;

      int& redundant = registry.addComponent<int>(entity, 6);

      Assert::AreEqual(5, redundant);
      Assert::AreEqual(&i.get(), &redundant);
    }

    TEST_METHOD(EntityRegistry_RemoveNonexistingComponent_NothingHappens) {
      TestEntityRegistry registry;
      LinearEntity entity = registry.createEntity();

      registry.removeComponent<int>(entity);

      Assert::IsFalse(registry.hasComponent<int>(entity));
    }
  };
}