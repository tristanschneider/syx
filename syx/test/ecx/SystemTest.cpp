#include "Precompile.h"
#include "CppUnitTest.h"

#include "EntityFactory.h"
#include "EntityModifier.h"
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
    using TestEntityFactory = EntityFactory<TestEntity>;
    template<class... Components>
    using TestEntityModifier = EntityModifier<TestEntity, Components...>;

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

    TEST_METHOD(SystemContext_BuildInfoWithDuplicates_NoDuplicates) {
      TestEntityRegistry registry;
      TestSystemContext<TestView<Read<int>, Write<int>, Include<int>, OptionalRead<int>, OptionalWrite<int>, Exclude<int>>, TestEntityModifier<int>> context(registry);

      const SystemInfo info = context.buildInfo();

      assertListsMatchOrderless(info.mExistenceTypes, { typeId<int, SystemInfo>() });
      assertListsMatchOrderless(info.mReadTypes, { typeId<int, SystemInfo>() });
      assertListsMatchOrderless(info.mWriteTypes, { typeId<int, SystemInfo>() });
      assertListsMatchOrderless(info.mFactoryTypes, { typeId<int, SystemInfo>() });
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

    TEST_METHOD(SystemContext_BuildInfoEntityFactory_UsesEntityFactory) {
      TestEntityRegistry registry;
      TestSystemContext<TestEntityFactory> context(registry);

      const SystemInfo info = context.buildInfo();

      Assert::IsTrue(info.mUsesEntityFactory);
    }

    TEST_METHOD(SystemContext_BuildInfoEntityModifier_InfoModifyReadWriteExistence) {
      TestEntityRegistry registry;
      TestSystemContext<TestEntityModifier<int>> context(registry);

      const SystemInfo info = context.buildInfo();

      assertListsMatchOrderless(info.mExistenceTypes, { typeId<int, SystemInfo>() });
      assertListsMatchOrderless(info.mReadTypes, { typeId<int, SystemInfo>() });
      assertListsMatchOrderless(info.mWriteTypes, { typeId<int, SystemInfo>() });
      assertListsMatchOrderless(info.mFactoryTypes, { typeId<int, SystemInfo>() });
      Assert::IsFalse(info.mUsesEntityFactory);
    }

    TEST_METHOD(SystemContext_UseEntityModifier_Works) {
      TestEntityRegistry registry;
      TestSystemContext<TestEntityModifier<int>> context(registry);
      auto entity = registry.createEntity();

      context.get<TestEntityModifier<int>>().addComponent<int>(entity, 6);

      const int* added = registry.tryGetComponent<int>(entity);
      Assert::IsNotNull(added);
      Assert::AreEqual(6, *added);
    }

    TEST_METHOD(SystemContext_GetEntityFactory_FactoryWorks) {
      TestEntityRegistry registry;
      TestSystemContext<TestEntityFactory> context(registry);

      context.get<TestEntityFactory>().createEntity();

      Assert::AreEqual(size_t(2), registry.size(), L"Created and singleton entities should exist");
    }

    TEST_METHOD(System_MakeSystem_BuildsSystemWithInfo) {
      auto system = makeSystem("", [](TestSystemContext<TestView<Read<int>>>&) {
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
      auto system = makeSystem("test", [](TestSystemContext<TestView<Write<int>>>& context) {
        for(auto entity : context.get<TestView<Write<int>>>()) {
          entity.get<int>()++;
        }
      });

      system->tick(registry);

      Assert::AreEqual(12, registry.getComponent<int>(entity));
    }

    TEST_METHOD(System_MakeSystem_HasName) {
      auto system = makeSystem("test", [](TestSystemContext<TestEntityFactory>&) {});

      const SystemInfo& info = system->getInfo();

      Assert::AreEqual(std::string("test"), info.mName);
    }
  };
}