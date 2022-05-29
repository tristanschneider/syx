#include "Precompile.h"
#include "CppUnitTest.h"

#include "SystemRegistry.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ecx {
  TEST_CLASS(SystemRegistryTest) {
    using TestEntity = uint32_t;
    using TestEntityRegistry = EntityRegistry<TestEntity>;
    using TestEntityFactory = EntityFactory<TestEntity>;
    using TestSystemRegistry = SystemRegistry<TestEntity>;
    template<class... Args>
    using TestSystemContext = SystemContext<TestEntity, Args...>;
    template<class... Args>
    using TestEntityModifier = EntityModifier<TestEntity, Args...>;
    template<class... Args>
    using TestView = View<TestEntity, Args...>;

    TEST_METHOD(SystemRegistry_TickRegisteredSystem_IsTicked) {
      struct TestSystem : public ISystem<TestEntity> {
        void tick(TestEntityRegistry&, ThreadLocalContext&) const override {
          (*mInvocations)++;
        }

        SystemInfo getInfo() const override { return {}; }

        std::shared_ptr<int> mInvocations = std::make_shared<int>(0);
      };
      TestEntityRegistry registry;
      TestSystemRegistry systems;
      auto system = std::make_unique<TestSystem>();
      auto invocations = system->mInvocations;
      systems.registerSystem(std::move(system));

      systems.tick(registry);

      Assert::AreEqual(1, *invocations);
    }

    TEST_METHOD(SystemRegister_RegisterTwoSystems_TickedInOrder) {
      struct SuccessComponent {};
      TestEntityRegistry registry;
      TestSystemRegistry systems;
      systems.registerSystem(makeSystem("first", [](TestSystemContext<TestEntityFactory, TestEntityModifier<int>>& context) {
        auto entity = context.get<TestEntityFactory>().createEntity();
        context.get<TestEntityModifier<int>>().addComponent<int>(entity, 5);
      }));
      systems.registerSystem(makeSystem("second", [](TestSystemContext<TestView<Read<int>>, TestEntityModifier<SuccessComponent>>& context) {
        for(auto entity : context.get<TestView<Read<int>>>()) {
          Assert::AreEqual(5, entity.get<const int>());
          context.get<TestEntityModifier<SuccessComponent>>().addComponent<SuccessComponent>(entity.entity());
        }
      }));

      systems.tick(registry);

      Assert::AreEqual(size_t(1), registry.size<SuccessComponent>());
    }
  };
}