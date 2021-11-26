#include "Precompile.h"
#include "CppUnitTest.h"

#include "EntityRegistry.h"

#include "View.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ecx {
  TEST_CLASS(EntityRegistryTest) {
    using TestEntityRegistry = EntityRegistry<uint32_t>;
    template<class... Args>
    using TestView = View<uint32_t, Args...>;

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
      Assert::AreEqual(entity, (*it).entity());
      Assert::AreEqual(10, (*it).get<const int>());
    }

    TEST_METHOD(View_WriteOne_IsFound) {
      TestEntityRegistry registry;
      auto entity = registry.createEntity();
      registry.addComponent<int>(entity, 10);
      auto view = TestView<Write<int>>(registry);

      auto it = view.begin();
      Assert::IsTrue(it != view.end());
      Assert::AreEqual(entity, (*it).entity());
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
      Assert::AreEqual(entity, (*it).entity());
      Assert::AreEqual(10, (*it).get<const int>());
    }

    TEST_METHOD(View_OptionalWriteOneMissingWithRead_IsFound) {
      TestEntityRegistry registry;
      auto entity = registry.createEntity();
      registry.addComponent<int>(entity, 10);
      auto view = TestView<Read<int>, OptionalWrite<short>>(registry);

      auto it = view.begin();
      Assert::IsTrue(it != view.end());
      Assert::AreEqual(entity, (*it).entity());
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
      Assert::AreEqual(entity, (*it).entity());
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
      Assert::AreEqual(entity, (*it).entity());
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
      Assert::AreEqual(entity, (*it).entity());
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
      Assert::AreEqual(entity, (*it).entity());
      Assert::AreEqual(10, (*it).get<const int>());
    }
  };
}