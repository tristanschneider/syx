#include "Precompile.h"
#include "CppUnitTest.h"

#include "EntityFactory.h"
#include "EntityModifier.h"
#include "LinearEntityRegistry.h"
#include "LinearView.h"
#include "System.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ecx {
  TEST_CLASS(LinearSystemTest) {
    using TestEntity = LinearEntity;
    template<class... Args>
    using TestSystemContext = SystemContext<TestEntity, Args...>;
    template<class... Args>
    using TestView = View<TestEntity, Args...>;
    using TestEntityFactory = EntityFactory<TestEntity>;
    template<class... Components>
    using TestEntityModifier = EntityModifier<TestEntity, Components...>;

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

    TEST_METHOD(SystemContext_GetView_ViewWorks) {
      TestEntityRegistry registry;
      auto entity = registry.createEntity();
      registry.addComponent<int>(entity, 10);
      ThreadLocalContext ctx;
      TestSystemContext<TestView<Read<int>>> context(registry, ctx);

      auto view = context.get<TestView<Read<int>>>();

      Assert::IsFalse(view.begin() == view.end());
      Assert::AreEqual(entity.mData.mParts.mEntityId, (*view.begin()).entity().mData.mParts.mEntityId);
    }

    TEST_METHOD(SystemContext_TwoViews_CanGetBoth) {
      TestEntityRegistry registry;
      ThreadLocalContext ctx;
      TestSystemContext<TestView<Read<int>>, TestView<Write<short>, Read<uint64_t>>> context(registry, ctx);

      context.get<TestView<Read<int>>>();
      context.get<TestView<Write<short>, Read<uint64_t>>>();
    }

    static void assertListsMatchOrderless(std::vector<typeId_t<TestEntity>> info, const std::initializer_list<typeId_t<TestEntity>>& expected) {
      std::sort(info.begin(), info.end());
      std::vector<typeId_t<TestEntity>> ex{ expected };
      std::sort(ex.begin(), ex.end());

      Assert::IsTrue(ex == info);
    }

    TEST_METHOD(SystemContext_BuildInfoInclude_InExistenceTypes) {
      TestEntityRegistry registry;
      ThreadLocalContext ctx;
      TestSystemContext<TestView<Read<int>, Include<short>>> context(registry, ctx);

      const SystemInfo info = context.buildInfo();

      assertListsMatchOrderless(info.mExistenceTypes, { typeId<int, TestEntity>(), typeId<short, TestEntity>() });
      Assert::AreEqual(size_t(1), info.mReadTypes.size());
      Assert::IsTrue(info.mWriteTypes.empty());
      Assert::IsTrue(info.mFactoryTypes.empty());
      Assert::IsFalse(info.mIsBlocking);
    }

    TEST_METHOD(SystemContext_BuildInfoWithDuplicates_NoDuplicates) {
      TestEntityRegistry registry;
      ThreadLocalContext ctx;
      TestSystemContext<TestView<Read<int>, Write<int>, Include<int>, OptionalRead<int>, OptionalWrite<int>, Exclude<int>>, TestEntityModifier<int>> context(registry, ctx);

      const SystemInfo info = context.buildInfo();

      assertListsMatchOrderless(info.mExistenceTypes, { typeId<int, TestEntity>() });
      assertListsMatchOrderless(info.mReadTypes, { typeId<int, TestEntity>() });
      assertListsMatchOrderless(info.mWriteTypes, { typeId<int, TestEntity>() });
      assertListsMatchOrderless(info.mFactoryTypes, { typeId<int, TestEntity>() });
    }

    TEST_METHOD(SystemContext_BuildInfoExclude_InExistenceTypes) {
      TestEntityRegistry registry;
      ThreadLocalContext ctx;
      TestSystemContext<TestView<Read<int>, Exclude<short>>> context(registry, ctx);

      const SystemInfo info = context.buildInfo();

      assertListsMatchOrderless(info.mExistenceTypes, { typeId<int, TestEntity>(), typeId<short, TestEntity>() });
      Assert::AreEqual(size_t(1), info.mReadTypes.size());
      Assert::IsTrue(info.mWriteTypes.empty());
      Assert::IsTrue(info.mFactoryTypes.empty());
      Assert::IsFalse(info.mIsBlocking);
    }

    TEST_METHOD(SystemContext_BuildInfoRead_InExistenceAndReadTypes) {
      TestEntityRegistry registry;
      ThreadLocalContext ctx;
      TestSystemContext<TestView<Read<int>>> context(registry, ctx);

      const SystemInfo info = context.buildInfo();

      assertListsMatchOrderless(info.mExistenceTypes, { typeId<int, TestEntity>() });
      assertListsMatchOrderless(info.mReadTypes, { typeId<int, TestEntity>() });
      Assert::IsTrue(info.mWriteTypes.empty());
      Assert::IsTrue(info.mFactoryTypes.empty());
      Assert::IsFalse(info.mIsBlocking);
    }

    TEST_METHOD(SystemContext_BuildInfoOptionalRead_InExistenceAndReadTypes) {
      TestEntityRegistry registry;
      ThreadLocalContext ctx;
      TestSystemContext<TestView<Read<int>, OptionalRead<short>>> context(registry, ctx);

      const SystemInfo info = context.buildInfo();

      assertListsMatchOrderless(info.mExistenceTypes, { typeId<int, TestEntity>(), typeId<short, TestEntity>() });
      assertListsMatchOrderless(info.mReadTypes, { typeId<int, TestEntity>(), typeId<short, TestEntity>() });
      Assert::IsTrue(info.mWriteTypes.empty());
      Assert::IsTrue(info.mFactoryTypes.empty());
      Assert::IsFalse(info.mIsBlocking);
    }

    TEST_METHOD(SystemContext_BuildInfoWrite_InExistenceAndReadAndWriteTypes) {
      TestEntityRegistry registry;
      ThreadLocalContext ctx;
      TestSystemContext<TestView<Write<int>>> context(registry, ctx);

      const SystemInfo info = context.buildInfo();

      assertListsMatchOrderless(info.mExistenceTypes, { typeId<int, TestEntity>() });
      assertListsMatchOrderless(info.mReadTypes, { typeId<int, TestEntity>() });
      assertListsMatchOrderless(info.mWriteTypes, { typeId<int, TestEntity>() });
      Assert::IsTrue(info.mFactoryTypes.empty());
      Assert::IsFalse(info.mIsBlocking);
    }

    TEST_METHOD(SystemContext_BuildInfoOptionalWrite_InExistenceAndReadAndWriteTypes) {
      TestEntityRegistry registry;
      ThreadLocalContext ctx;
      TestSystemContext<TestView<Write<int>, OptionalWrite<short>>> context(registry, ctx);

      const SystemInfo info = context.buildInfo();

      assertListsMatchOrderless(info.mExistenceTypes, { typeId<int, TestEntity>(), typeId<short, TestEntity>() });
      assertListsMatchOrderless(info.mReadTypes, { typeId<int, TestEntity>(), typeId<short, TestEntity>() });
      assertListsMatchOrderless(info.mWriteTypes, { typeId<int, TestEntity>(), typeId<short, TestEntity>() });
      Assert::IsTrue(info.mFactoryTypes.empty());
      Assert::IsFalse(info.mIsBlocking);
    }

    TEST_METHOD(SystemContext_BuildInfoEntityFactory_UsesEntityFactory) {
      TestEntityRegistry registry;
      ThreadLocalContext ctx;
      TestSystemContext<TestEntityFactory> context(registry, ctx);

      const SystemInfo info = context.buildInfo();

      Assert::IsTrue(info.mIsBlocking);
    }

    TEST_METHOD(SystemContext_BuildInfoEntityModifier_InfoModifyReadWriteExistence) {
      TestEntityRegistry registry;
      ThreadLocalContext ctx;
      TestSystemContext<TestEntityModifier<int>> context(registry, ctx);

      const SystemInfo info = context.buildInfo();

      assertListsMatchOrderless(info.mExistenceTypes, { typeId<int, TestEntity>() });
      assertListsMatchOrderless(info.mReadTypes, { typeId<int, TestEntity>() });
      assertListsMatchOrderless(info.mWriteTypes, { typeId<int, TestEntity>() });
      assertListsMatchOrderless(info.mFactoryTypes, { typeId<int, TestEntity>() });
      Assert::IsFalse(info.mIsBlocking);
    }

    TEST_METHOD(SystemContext_UseEntityModifier_Works) {
      TestEntityRegistry registry;
      ThreadLocalContext ctx;
      TestSystemContext<TestEntityModifier<int>> context(registry, ctx);
      auto entity = registry.createEntity();

      context.get<TestEntityModifier<int>>().addComponent<int>(entity, 6);

      const int* added = registry.tryGetComponent<int>(entity);
      Assert::IsNotNull(added);
      Assert::AreEqual(6, *added);
    }

    TEST_METHOD(SystemContext_GetEntityFactory_FactoryWorks) {
      TestEntityRegistry registry;
      ThreadLocalContext ctx;
      TestSystemContext<TestEntityFactory> context(registry, ctx);

      context.get<TestEntityFactory>().createEntity();

      Assert::AreEqual(size_t(2), registry.size(), L"Created and singleton entities should exist");
    }

    TEST_METHOD(SystemContext_ReUseView_SameValue) {
      TestEntityRegistry registry;
      ThreadLocalContext ctx;
      TestSystemContext<TestView<Read<int>>> context(registry, ctx);
      auto e = registry.createEntity();
      registry.addComponent<int>(e, 1);
      auto view = context.get<TestView<Read<int>>>();
      view = context.get<TestView<Read<int>>>();

      Assert::IsTrue((*view.begin()).entity() == e);
    }

    TEST_METHOD(SystemContext_InvalidatedReUseView_ContainsNewValues) {
      TestEntityRegistry registry;
      ThreadLocalContext ctx;
      TestSystemContext<TestView<Read<int>>> context(registry, ctx);
      auto e = registry.createEntity();
      registry.addComponent<int>(e, 1);
      auto& view = context.get<TestView<Read<int>>>();

      //Invalidate view
      registry.addComponent<short>(e, short(2));

      auto& newView = context.get<TestView<Read<int>>>();

      Assert::IsTrue(&view == &newView);
      Assert::IsTrue((*view.begin()).entity().mData.mParts.mEntityId == e.mData.mParts.mEntityId);
    }

    TEST_METHOD(System_MakeSystem_BuildsSystemWithInfo) {
      auto system = makeSystem("", [](TestSystemContext<TestView<Read<int>>>&) {
      });

      const SystemInfo info = system->getInfo();

      assertListsMatchOrderless(info.mExistenceTypes, { typeId<int, TestEntity>() });
      assertListsMatchOrderless(info.mReadTypes, { typeId<int, TestEntity>() });
      Assert::IsTrue(info.mWriteTypes.empty());
      Assert::IsTrue(info.mFactoryTypes.empty());
      Assert::IsFalse(info.mIsBlocking);
    }

    TEST_METHOD(System_ExplicitSystem_BuildsSystemWithInfo) {
      using TestContext = TestSystemContext<TestView<Read<int>>>;
      struct TestSystem : public System<TestContext, TestEntity> {
        void _tick(TestContext&) const override {
        }
      };

      const SystemInfo info = TestSystem().getInfo();

      assertListsMatchOrderless(info.mExistenceTypes, { typeId<int, TestEntity>() });
      assertListsMatchOrderless(info.mReadTypes, { typeId<int, TestEntity>() });
      Assert::IsTrue(info.mWriteTypes.empty());
      Assert::IsTrue(info.mFactoryTypes.empty());
      Assert::IsFalse(info.mIsBlocking);
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

      ThreadLocalContext ctx;
      system->tick(registry, ctx);

      Assert::AreEqual(12, registry.getComponent<int>(entity));
    }

    TEST_METHOD(System_MakeSystem_HasName) {
      auto system = makeSystem("test", [](TestSystemContext<TestEntityFactory>&) {});

      const SystemInfo<TestEntity>& info = system->getInfo();

      Assert::AreEqual(std::string("test"), info.mName);
    }
  };
}