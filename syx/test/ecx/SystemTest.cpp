#include "Precompile.h"
#include "CppUnitTest.h"

#include "System.h"
#include "View.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ecx {
  TEST_CLASS(SystemTest) {
    using TestEntity = uint32_t;
    using TestEntityRegistry = EntityRegistry<TestEntity>;
    template<class... Args>
    using TestSystemContext = SystemContext<TestEntity, Args...>;
    template<class... Args>
    using TestView = View<TestEntity, Args...>;

    TEST_METHOD(SystemContext_GetView_ViewWorks) {
      TestEntityRegistry registry;
      auto entity = registry.createEntity();
      registry.addComponent<int>(entity, 10);
      TestSystemContext<TestView<Read<int>>> context(registry);

      auto view = context.get<TestView<Read<int>>>();

      Assert::IsFalse(view.begin() == view.end());
      Assert::AreEqual(entity, (*view.begin()).entity());
    }

    TEST_METHOD(SystemContext_TwoViews_CanGetBoth) {
      TestEntityRegistry registry;
      TestSystemContext<TestView<Read<int>>, TestView<Write<short>, Read<uint64_t>>> context(registry);

      context.get<TestView<Read<int>>>();
      context.get<TestView<Write<short>, Read<uint64_t>>>();
    }

    static void assertListsMatchOrderless(std::vector<typeId_t<SystemInfo>> info, const std::initializer_list<typeId_t<SystemInfo>>& expected) {
      std::sort(info.begin(), info.end());
      std::vector<typeId_t<SystemInfo>> ex{ expected };
      std::sort(ex.begin(), ex.end());

      Assert::IsTrue(ex == info);
    }

    TEST_METHOD(SystemContext_BuildInfoInclude_InExistenceTypes) {
      TestEntityRegistry registry;
      TestSystemContext<TestView<Read<int>, Include<short>>> context(registry);

      const SystemInfo info = context.buildInfo();

      assertListsMatchOrderless(info.mExistenceTypes, { typeId<int, SystemInfo>(), typeId<short, SystemInfo>() });
      Assert::AreEqual(size_t(1), info.mReadTypes.size());
      Assert::IsTrue(info.mWriteTypes.empty());
      Assert::IsTrue(info.mFactoryTypes.empty());
      Assert::IsFalse(info.mUsesEntityFactory);
    }

    TEST_METHOD(SystemContext_BuildInfoExclude_InExistenceTypes) {
      TestEntityRegistry registry;
      TestSystemContext<TestView<Read<int>, Exclude<short>>> context(registry);

      const SystemInfo info = context.buildInfo();

      assertListsMatchOrderless(info.mExistenceTypes, { typeId<int, SystemInfo>(), typeId<short, SystemInfo>() });
      Assert::AreEqual(size_t(1), info.mReadTypes.size());
      Assert::IsTrue(info.mWriteTypes.empty());
      Assert::IsTrue(info.mFactoryTypes.empty());
      Assert::IsFalse(info.mUsesEntityFactory);
    }

    TEST_METHOD(SystemContext_BuildInfoRead_InExistenceAndReadTypes) {
      TestEntityRegistry registry;
      TestSystemContext<TestView<Read<int>>> context(registry);

      const SystemInfo info = context.buildInfo();

      assertListsMatchOrderless(info.mExistenceTypes, { typeId<int, SystemInfo>() });
      assertListsMatchOrderless(info.mReadTypes, { typeId<int, SystemInfo>() });
      Assert::IsTrue(info.mWriteTypes.empty());
      Assert::IsTrue(info.mFactoryTypes.empty());
      Assert::IsFalse(info.mUsesEntityFactory);
    }

    TEST_METHOD(SystemContext_BuildInfoOptionalRead_InExistenceAndReadTypes) {
      TestEntityRegistry registry;
      TestSystemContext<TestView<Read<int>, OptionalRead<short>>> context(registry);

      const SystemInfo info = context.buildInfo();

      assertListsMatchOrderless(info.mExistenceTypes, { typeId<int, SystemInfo>(), typeId<short, SystemInfo>() });
      assertListsMatchOrderless(info.mReadTypes, { typeId<int, SystemInfo>(), typeId<short, SystemInfo>() });
      Assert::IsTrue(info.mWriteTypes.empty());
      Assert::IsTrue(info.mFactoryTypes.empty());
      Assert::IsFalse(info.mUsesEntityFactory);
    }

    TEST_METHOD(SystemContext_BuildInfoWrite_InExistenceAndReadAndWriteTypes) {
      TestEntityRegistry registry;
      TestSystemContext<TestView<Write<int>>> context(registry);

      const SystemInfo info = context.buildInfo();

      assertListsMatchOrderless(info.mExistenceTypes, { typeId<int, SystemInfo>() });
      assertListsMatchOrderless(info.mReadTypes, { typeId<int, SystemInfo>() });
      assertListsMatchOrderless(info.mWriteTypes, { typeId<int, SystemInfo>() });
      Assert::IsTrue(info.mFactoryTypes.empty());
      Assert::IsFalse(info.mUsesEntityFactory);
    }

    TEST_METHOD(SystemContext_BuildInfoOptionalWrite_InExistenceAndReadAndWriteTypes) {
      TestEntityRegistry registry;
      TestSystemContext<TestView<Write<int>, OptionalWrite<short>>> context(registry);

      const SystemInfo info = context.buildInfo();

      assertListsMatchOrderless(info.mExistenceTypes, { typeId<int, SystemInfo>(), typeId<short, SystemInfo>() });
      assertListsMatchOrderless(info.mReadTypes, { typeId<int, SystemInfo>(), typeId<short, SystemInfo>() });
      assertListsMatchOrderless(info.mWriteTypes, { typeId<int, SystemInfo>(), typeId<short, SystemInfo>() });
      Assert::IsTrue(info.mFactoryTypes.empty());
      Assert::IsFalse(info.mUsesEntityFactory);
    }

    TEST_METHOD(System_MakeSystem_BuildsSystemWithInfo) {
      auto system = makeSystem([](TestSystemContext<TestView<Read<int>>>&) {
      });

      const SystemInfo info = system->getInfo();

      assertListsMatchOrderless(info.mExistenceTypes, { typeId<int, SystemInfo>() });
      assertListsMatchOrderless(info.mReadTypes, { typeId<int, SystemInfo>() });
      Assert::IsTrue(info.mWriteTypes.empty());
      Assert::IsTrue(info.mFactoryTypes.empty());
      Assert::IsFalse(info.mUsesEntityFactory);
    }

    TEST_METHOD(System_ExplicitSystem_BuildsSystemWithInfo) {
      using TestContext = TestSystemContext<TestView<Read<int>>>;
      struct TestSystem : public System<TestContext, TestEntity> {
        void _tick(TestContext&) const override {
        }
      };

      const SystemInfo info = TestSystem().getInfo();

      assertListsMatchOrderless(info.mExistenceTypes, { typeId<int, SystemInfo>() });
      assertListsMatchOrderless(info.mReadTypes, { typeId<int, SystemInfo>() });
      Assert::IsTrue(info.mWriteTypes.empty());
      Assert::IsTrue(info.mFactoryTypes.empty());
      Assert::IsFalse(info.mUsesEntityFactory);
    }

    TEST_METHOD(System_Tick_ComponentValueChanges) {
      TestEntityRegistry registry;
      auto entity = registry.createEntity();
      registry.addComponent<int>(entity, 11);
      auto system = makeSystem([](TestSystemContext<TestView<Write<int>>>& context) {
        for(auto entity : context.get<TestView<Write<int>>>()) {
          entity.get<int>()++;
        }
      });

      system->tick(registry);

      Assert::AreEqual(12, registry.getComponent<int>(entity));
    }
  };
}