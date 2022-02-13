#include "Precompile.h"
#include "CppUnitTest.h"

#include "LinearView.h"
#include "LinearEntityRegistry.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ecx {
  TEST_CLASS(LinearViewTest) {
    using TestEntity = LinearEntity;
    using TestEntityRegistry = EntityRegistry<LinearEntity>;
    template<class... Args>
    using TestView = View<LinearEntity, Args...>;

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
      Assert::AreEqual(1, storageA->at(0));
      Assert::AreEqual(2, storageB->at(0));
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
  };
}