#include "Precompile.h"
#include "CppUnitTest.h"

#include "BlockVectorTypeErasedContainerTraits.h"
#include "LinearView.h"
#include "LinearEntityRegistry.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ecx {
  TEST_CLASS(LinearViewTest) {
    using TestEntity = LinearEntity;
    template<class... Args>
    using TestView = View<LinearEntity, Args...>;

    struct TestEntityRegistry : public EntityRegistry<LinearEntity> {
      using Base = EntityRegistry<LinearEntity>;

      LinearEntity createEntity() {
        return Base::createEntity(*getDefaultEntityGenerator());
      }

      void destroyEntity(LinearEntity entity) {
        Base::destroyEntity(entity, *getDefaultEntityGenerator());
      }

      template<class... Args>
      auto createEntityWithComponents() {
        return Base::createEntityWithComponents<Args...>(*getDefaultEntityGenerator());
      }

      template<class... Args>
      auto createAndGetEntityWithComponents() {
        return Base::createAndGetEntityWithComponents<Args...>(*getDefaultEntityGenerator());
      }
    };

    template<class... Args>
    struct Test;

    static_assert(std::is_same_v<
      ViewDeducer::ViewTraits<Read<int>>::ApplyAllowedTypes<Test>::type,
      Test<const int>
    >);
    static_assert(std::is_same_v<
      ViewDeducer::ViewTraits<Read<int>, Write<double>>::ApplyAllowedTypes<Test>::type,
      Test<const int, double>
    >);
    static_assert(std::is_same_v<
      ViewDeducer::ViewTraits<Exclude<int>>::ApplyAllowedTypes<Test>::type,
      Test<>
    >);
    static_assert(std::is_same_v<
      ViewDeducer::ViewTraits<OptionalRead<int>>::ApplyAllowedTypes<Test>::type,
      Test<const int>
    >);
    static_assert(std::is_same_v<
      ViewDeducer::ViewTraits<OptionalWrite<int>>::ApplyAllowedTypes<Test>::type,
      Test<int>
    >);
    static_assert(std::is_same_v<
      ViewDeducer::ViewTraits<Include<int>>::ApplyAllowedTypes<Test>::type,
      Test<>
    >);
    static_assert(std::is_same_v<
      ViewDeducer::ViewTraits<Write<int>>::ApplyAllowedTypes<Test>::type,
      Test<int>
    >);
    static_assert(std::is_same_v<
      ViewDeducer::ViewTraits<Read<int>, Write<double>, OptionalRead<float>, Exclude<char>, Include<std::string>>::ApplyAllowedTypes<Test>::type,
      Test<const int, double, const float>
    >);

    TEST_METHOD(View_Empty_BeginIsEnd) {
      TestEntityRegistry registry;
      auto view = TestView<Read<int>>(registry);

      Assert::IsTrue(view.begin() == view.end());
    }

    TEST_METHOD(View_ReadOne_IsFound) {
      TestEntityRegistry registry;
      auto entity = registry.createEntity();
      registry.addComponent<int>(entity, 10);
      auto view = TestView<Read<int>>(registry);

      auto it = view.begin();
      Assert::IsTrue(it != view.end());
      Assert::AreEqual(entity.mData.mParts.mEntityId, (*it).entity().mData.mParts.mEntityId);
      Assert::AreEqual(10, (*it).get<const int>());
    }

    TEST_METHOD(View_WriteOne_IsFound) {
      TestEntityRegistry registry;
      auto entity = registry.createEntity();
      registry.addComponent<int>(entity, 10);
      auto view = TestView<Write<int>>(registry);

      auto it = view.begin();
      Assert::IsTrue(it != view.end());
      Assert::AreEqual(entity.mData.mParts.mEntityId, (*it).entity().mData.mParts.mEntityId);
      Assert::AreEqual(10, (*it).get<int>());
    }

    TEST_METHOD(View_OptionalReadOneEmpty_NotFound) {
      TestEntityRegistry registry;
      registry.createEntity();
      auto view = TestView<OptionalRead<int>>(registry);

      Assert::IsTrue(view.begin() == view.end());
    }

    TEST_METHOD(View_OptionalWriteOneEmpty_NotFound) {
      TestEntityRegistry registry;
      registry.createEntity();
      auto view = TestView<OptionalWrite<int>>(registry);

      Assert::IsTrue(view.begin() == view.end());
    }

    TEST_METHOD(View_OptionalReadOneMissingWithRead_IsFound) {
      TestEntityRegistry registry;
      auto entity = registry.createEntity();
      registry.addComponent<int>(entity, 10);
      auto view = TestView<Read<int>, OptionalRead<short>>(registry);

      auto it = view.begin();
      Assert::IsTrue(it != view.end());
      Assert::AreEqual(entity.mData.mParts.mEntityId, (*it).entity().mData.mParts.mEntityId);
      Assert::AreEqual(10, (*it).get<const int>());
    }

    TEST_METHOD(View_OptionalWriteOneMissingWithRead_IsFound) {
      TestEntityRegistry registry;
      auto entity = registry.createEntity();
      registry.addComponent<int>(entity, 10);
      auto view = TestView<Read<int>, OptionalWrite<short>>(registry);

      auto it = view.begin();
      Assert::IsTrue(it != view.end());
      Assert::AreEqual(entity.mData.mParts.mEntityId, (*it).entity().mData.mParts.mEntityId);
      Assert::AreEqual(10, (*it).get<const int>());
    }

    TEST_METHOD(View_OptionalReadOneWithRead_IsFound) {
      TestEntityRegistry registry;
      auto entity = registry.createEntity();
      registry.addComponent<int>(entity, 10);
      registry.addComponent<short>(entity, short(5));
      auto view = TestView<Read<int>, OptionalRead<short>>(registry);

      auto it = view.begin();
      Assert::IsTrue(it != view.end());
      Assert::AreEqual(entity.mData.mParts.mEntityId, (*it).entity().mData.mParts.mEntityId);
      Assert::AreEqual(10, (*it).get<const int>());
      Assert::AreEqual(short(5), (*it).get<const short>());
    }

    TEST_METHOD(View_OptionalWriteOneWithRead_IsFound) {
      TestEntityRegistry registry;
      auto entity = registry.createEntity();
      registry.addComponent<int>(entity, 10);
      registry.addComponent<short>(entity, short(5));
      auto view = TestView<Read<int>, OptionalWrite<short>>(registry);

      auto it = view.begin();
      Assert::IsTrue(it != view.end());
      Assert::AreEqual(entity.mData.mParts.mEntityId, (*it).entity().mData.mParts.mEntityId);
      Assert::AreEqual(10, (*it).get<const int>());
      Assert::AreEqual(short(5), (*it).get<short>());
    }

    TEST_METHOD(View_ExcludeOneContains_NotFound) {
      TestEntityRegistry registry;
      auto entity = registry.createEntity();
      registry.addComponent<int>(entity, 10);
      registry.addComponent<short>(entity);
      auto view = TestView<Read<int>, Exclude<short>>(registry);

      Assert::IsTrue(view.begin() == view.end());
    }

    TEST_METHOD(View_ExcludeOneExcluded_Found) {
      TestEntityRegistry registry;
      auto entity = registry.createEntity();
      registry.addComponent<int>(entity, 10);
      auto view = TestView<Read<int>, Exclude<short>>(registry);

      auto it = view.begin();
      Assert::IsTrue(it != view.end());
      Assert::AreEqual(entity.mData.mParts.mEntityId, (*it).entity().mData.mParts.mEntityId);
      Assert::AreEqual(10, (*it).get<const int>());
    }

    TEST_METHOD(View_IncludeOneMissing_NotFound) {
      TestEntityRegistry registry;
      auto entity = registry.createEntity();
      registry.addComponent<int>(entity, 10);
      auto view = TestView<Read<int>, Include<short>>(registry);

      Assert::IsTrue(view.begin() == view.end());
    }

    TEST_METHOD(View_IncludeOneIncluded_Found) {
      TestEntityRegistry registry;
      auto entity = registry.createEntity();
      registry.addComponent<int>(entity, 10);
      registry.addComponent<short>(entity);
      auto view = TestView<Read<int>, Include<short>>(registry);

      auto it = view.begin();
      Assert::IsTrue(it != view.end());
      Assert::AreEqual(entity.mData.mParts.mEntityId, (*it).entity().mData.mParts.mEntityId);
      Assert::AreEqual(10, (*it).get<const int>());
    }

    TEST_METHOD(View_RangeIterator_AllFound) {
      TestEntityRegistry registry;
      for(int i = 0; i < 2; ++i) {
        auto entity = registry.createEntity();
        registry.addComponent<int>(entity, i);
      }
      int found = 0;
      auto view = TestView<Write<int>>(registry);

      for(auto entity : view) {
        Assert::IsTrue(entity.hasComponent<int>());
        ++found;
      }

      Assert::AreEqual(2, found);
    }

    TEST_METHOD(ViewWithEmptyChunk_Iterate_AllFound) {
      TestEntityRegistry registry;
      TestEntity entity = registry.createEntity();
      registry.addComponent<int>(entity, 1);
      entity = registry.createEntityWithComponents<int, uint64_t>();
      registry.removeComponent<uint64_t>(entity);
      registry.getComponent<int>(entity) = 1;

      TestView<Read<int>> view(registry);
      size_t found = 0;
      for(auto e : view) {
        Assert::AreEqual(1, e.get<const int>());
        ++found;
      }
      Assert::AreEqual(size_t(2), found);
    }

    TEST_METHOD(ChunkView_EntitiesAcrossChunks_AllFound) {
      TestEntityRegistry registry;
      auto a = registry.createEntity();
      registry.addComponent<int>(a, 1);
      auto b = registry.createEntity();
      registry.addComponent<int>(b, 2);
      registry.addComponent<short>(b, short(1));
      auto view = TestView<Read<int>>(registry);

      auto chunkIt = view.chunksBegin();
      const std::vector<int>* storageA = (*chunkIt).tryGet<const int>();
      const std::vector<int>* storageB = (*++chunkIt).tryGet<const int>();

      Assert::IsNotNull(storageA);
      Assert::IsNotNull(storageB);
      Assert::AreEqual(size_t(1), storageA->size());
      Assert::AreEqual(size_t(1), storageB->size());
      //Order isn't important, make sure both values are found
      const int storedA = storageA->at(0);
      const int storedB = storageB->at(0);
      Assert::IsTrue(storedA == 1 || storedA == 2);
      Assert::IsTrue(storedB == 1 || storedB == 2);
    }

    TEST_METHOD(ChunkView_MultipleInSameChunk_ContiguousValues) {
      TestEntityRegistry registry;
      std::vector<LinearEntity> entities;
      for(int i = 0; i < 10; ++i) {
        auto e = registry.createEntity();
        registry.addComponent<int>(e, i);
        entities.push_back(e);
      }
      auto view = TestView<Read<int>>(registry);
      auto chunkIt = view.chunksBegin();

      const std::vector<int>* storage = (*chunkIt).tryGet<const int>();
      Assert::IsNotNull(storage);
      Assert::AreEqual(size_t(10), storage->size());
      for(int i = 0; i < 10; ++i) {
        Assert::AreEqual(i, storage->at(i));
        Assert::AreEqual(entities[i].mData.mParts.mEntityId, (*chunkIt).indexToEntity(size_t(i)).mData.mParts.mEntityId);
        Assert::AreEqual(size_t(i), (*chunkIt).entityToIndex(entities[i]));
        Assert::AreEqual(i, *(*chunkIt).tryGetComponent<const int>(entities[i]));
      }
    }

    TEST_METHOD(ChunkView_EmptyView_BeginIsEnd) {
      TestEntityRegistry registry;
      auto view = TestView<Read<int>>(registry);

      Assert::IsTrue(view.chunksBegin() == view.chunksEnd());
    }

    TEST_METHOD(ChunkView_RecentlyEmptyView_BeginIsEnd) {
      TestEntityRegistry registry;
      auto e = registry.createEntity();
      registry.addComponent<int>(e, 1);
      registry.destroyEntity(e);
      auto view = TestView<Read<int>>(registry);

      Assert::IsTrue(view.chunksBegin() == view.chunksEnd());
    }

    TEST_METHOD(RuntimeView_Empty_BeginIsEnd) {
      TestEntityRegistry registry;
      ViewedTypes t;
      t.mIncludes.push_back(ecx::typeId<int, LinearEntity>());
      RuntimeView<TestEntity, true> view(registry, t);

      Assert::IsTrue(view.begin() == view.end());
    }

    TEST_METHOD(RuntimeView_FindExisting_IsFound) {
      TestEntityRegistry registry;
      ViewedTypes t;
      t.mIncludes.push_back(ecx::typeId<int, LinearEntity>());
      auto entity = registry.createEntityWithComponents<int, unsigned>();
      RuntimeView<TestEntity, true> view(registry, t);

      auto it = view.find(entity);

      Assert::IsTrue(it != view.end());
    }

    TEST_METHOD(RuntimeView_FindNotExisting_IsEnd) {
      TestEntityRegistry registry;
      ViewedTypes t;
      t.mIncludes.push_back(ecx::typeId<int, LinearEntity>());
      auto entity = registry.createEntityWithComponents<int, unsigned>();
      auto unrelated = registry.createEntityWithComponents<unsigned>();
      RuntimeView<TestEntity, true> view(registry, t);

      auto it = view.find(unrelated);

      Assert::IsTrue(it == view.end());
    }

    TEST_METHOD(RuntimeView_TryGetRead_ReturnsValues) {
      TestEntityRegistry registry;
      ViewedTypes t;
      t.mReads.push_back(ecx::typeId<int, LinearEntity>());
      t.mOptionalReads.push_back(ecx::typeId<unsigned, LinearEntity>());
      auto entityA = registry.createEntityWithComponents<int, unsigned>();
      auto entityB = registry.createEntityWithComponents<int>();
      RuntimeView<TestEntity, true> view(registry, t);

      auto it = view.find(entityA);
      Assert::IsNotNull(it.tryGet<const int>());
      Assert::IsNotNull(it.tryGet<const unsigned>());
      Assert::IsNull(it.tryGet<const float>());
      it = view.find(entityB);
      Assert::IsNotNull(it.tryGet<const int>());
      Assert::IsNull(it.tryGet<const unsigned>());
      Assert::IsNull(it.tryGet<const float>());
    }

    TEST_METHOD(RuntimeView_TryGetWrite_ReturnsValues) {
      TestEntityRegistry registry;
      ViewedTypes t;
      t.mWrites.push_back(ecx::typeId<int, LinearEntity>());
      t.mOptionalWrites.push_back(ecx::typeId<unsigned, LinearEntity>());
      auto entityA = registry.createEntityWithComponents<int, unsigned>();
      auto entityB = registry.createEntityWithComponents<int>();
      RuntimeView<TestEntity, true> view(registry, t);

      auto it = view.find(entityA);
      Assert::IsNotNull(it.tryGet<const int>());
      Assert::IsNotNull(it.tryGet<const unsigned>());
      Assert::IsNull(it.tryGet<const float>());
      Assert::IsNotNull(it.tryGet<int>());
      Assert::IsNotNull(it.tryGet<unsigned>());
      Assert::IsNull(it.tryGet<float>());
      it = view.find(entityB);
      Assert::IsNotNull(it.tryGet<const int>());
      Assert::IsNull(it.tryGet<const unsigned>());
      Assert::IsNull(it.tryGet<const float>());
      Assert::IsNotNull(it.tryGet<int>());
      Assert::IsNull(it.tryGet<unsigned>());
      Assert::IsNull(it.tryGet<float>());
    }

    TEST_METHOD(RuntimeViewNoSafety_TryGetRead_ReturnsValues) {
      TestEntityRegistry registry;
      ViewedTypes t;
      t.mReads.push_back(ecx::typeId<int, LinearEntity>());
      t.mOptionalReads.push_back(ecx::typeId<unsigned, LinearEntity>());
      auto entityA = registry.createEntityWithComponents<int, unsigned>();
      auto entityB = registry.createEntityWithComponents<int>();
      RuntimeView<TestEntity, false> view(registry, t);

      auto it = view.find(entityA);
      Assert::IsNotNull(it.tryGet<const int>());
      Assert::IsNotNull(it.tryGet<const unsigned>());
      Assert::IsNull(it.tryGet<const float>());
      it = view.find(entityB);
      Assert::IsNotNull(it.tryGet<const int>());
      Assert::IsNull(it.tryGet<const unsigned>());
      Assert::IsNull(it.tryGet<const float>());
    }

    TEST_METHOD(RuntimeViewNoSafety_TryGetWrite_ReturnsValues) {
      TestEntityRegistry registry;
      ViewedTypes t;
      t.mWrites.push_back(ecx::typeId<int, LinearEntity>());
      t.mOptionalWrites.push_back(ecx::typeId<unsigned, LinearEntity>());
      auto entityA = registry.createEntityWithComponents<int, unsigned>();
      auto entityB = registry.createEntityWithComponents<int>();
      RuntimeView<TestEntity, false> view(registry, t);

      auto it = view.find(entityA);
      Assert::IsNotNull(it.tryGet<const int>());
      Assert::IsNotNull(it.tryGet<const unsigned>());
      Assert::IsNull(it.tryGet<const float>());
      Assert::IsNotNull(it.tryGet<int>());
      Assert::IsNotNull(it.tryGet<unsigned>());
      Assert::IsNull(it.tryGet<float>());
      it = view.find(entityB);
      Assert::IsNotNull(it.tryGet<const int>());
      Assert::IsNull(it.tryGet<const unsigned>());
      Assert::IsNull(it.tryGet<const float>());
      Assert::IsNotNull(it.tryGet<int>());
      Assert::IsNull(it.tryGet<unsigned>());
      Assert::IsNull(it.tryGet<float>());
    }

    TEST_METHOD(RuntimeView_Iterate_ExposesAllValues) {
      TestEntityRegistry registry;
      ViewedTypes t;
      t.mReads.push_back(ecx::typeId<int, LinearEntity>());
      std::unordered_set<int> expected;
      for(int i = 0; i < 10; ++i) {
        auto e = registry.createEntityWithComponents<int>();
        registry.getComponent<int>(e) = i;

        //Put the values in multiple chunks
        if(i % 2) {
          registry.addComponent<unsigned>(e);
        }
        expected.insert(i);
      }

      RuntimeView<TestEntity, false> view(registry, t);
      std::unordered_set<int> found;
      for(auto it = view.begin(); it != view.end(); ++it) {
        const int* value = it.tryGet<const int>();
        Assert::IsNotNull(value);
        found.insert(*value);
      }

      Assert::IsTrue(expected == found);
    }

    struct TestComponent {
      int mInt = 7;
      std::string mStr = "str";
    };

    TEST_METHOD(RuntimeView_ViewCustomStorage_ValuesFound) {
      BlockVectorTraits<TestComponent> bvTraits;
      BlockVectorTypeErasedContainerTraits<> containerTraits(bvTraits);
      TestEntityRegistry registry;
      auto entity = registry.createEntity();
      auto customIdA = claimTypeId<LinearEntity>();
      auto customIdB = claimTypeId<LinearEntity>();
      TestComponent* resultA = static_cast<TestComponent*>(registry.addRuntimeComponent(entity, customIdA, &BlockVectorTypeErasedContainerTraits<>::createStorage, &containerTraits).second);
      resultA->mInt = 1;
      resultA->mStr = "a";
      TestComponent* resultB = static_cast<TestComponent*>(registry.addRuntimeComponent(entity, customIdB, &BlockVectorTypeErasedContainerTraits<>::createStorage, &containerTraits).second);
      resultB->mInt = 2;
      resultB->mStr = "b";
      ecx::ViewedTypes types;
      types.mReads.push_back(customIdA);
      RuntimeView<TestEntity, true> viewA(registry, types);
      types.mReads[0] = customIdB;
      RuntimeView<TestEntity, true> viewB(registry, types);

      auto it = viewA.begin();
      Assert::IsFalse(it == viewA.end());
      resultA = static_cast<TestComponent*>(it.tryGet(customIdA, ViewAccessMode::Read));
      Assert::IsNotNull(resultA);
      Assert::AreEqual(1, resultA->mInt);
      Assert::AreEqual("a", resultA->mStr.c_str());

      it = viewB.begin();
      Assert::IsFalse(it == viewB.end());
      resultB = static_cast<TestComponent*>(it.tryGet(customIdB, ViewAccessMode::Read));
      Assert::IsNotNull(resultB);
      Assert::AreEqual(2, resultB->mInt);
      Assert::AreEqual("b", resultB->mStr.c_str());
    }
  };
}