#include "Precompile.h"
#include "CppUnitTest.h"

#include "ecs/ECS.h"
#include "CommandBuffer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ecx {
  TEST_CLASS(CommandBufferTest) {
    using TestBuffer = CommandBuffer<LinearEntity>;
    using TestRegistry = EntityRegistry<LinearEntity>;
    using TestTypeA = int32_t;
    using TestTypeB = int64_t;

    struct Registry {
      enum class Mode {
        All,
        Specific,
      };

      void processAllCommands() {
        mBuffer.processAllCommands(mRegistry);
      }

      template<class T>
      void processCommandsForComponent() {
        mBuffer.processCommandsForComponent<T>(mRegistry);
      }

      void process() {
        switch(mMode) {
        case Mode::All:
          processAllCommands();
          break;
        case Mode::Specific:
          processCommandsForComponent<TestTypeA>();
          processCommandsForComponent<TestTypeB>();
          break;
        }
      }

      static std::function<LinearEntity()> createIDGenerator() {
        return [g(std::make_shared<uint32_t>(100))] {
          return LinearEntity(++(*g), 0);
        };
      }

      Mode mMode = Mode::All;
      TestRegistry mRegistry;
      TestBuffer mBuffer{ createIDGenerator() };
    };

    std::unique_ptr<Registry> mAllRegistry;
    std::unique_ptr<Registry> mSpecificRegistry;

    TEST_METHOD_INITIALIZE(init) {
      mAllRegistry = std::make_unique<Registry>();
      mSpecificRegistry = std::make_unique<Registry>();
      mAllRegistry->mMode = Registry::Mode::All;
      mSpecificRegistry->mMode = Registry::Mode::Specific;
    }

    void _commandBuffer_CreateEmptyEntity_Exists(Registry& registry) {
      auto&& [entity] = registry.mBuffer.createAndGetEntityWithComponents<>();

      registry.process();

      Assert::IsTrue(registry.mRegistry.isValid(entity));
    }

    TEST_METHOD(CommandBuffer_CreateEmptyEntity_Exists) {
      _commandBuffer_CreateEmptyEntity_Exists(*mAllRegistry);
      _commandBuffer_CreateEmptyEntity_Exists(*mSpecificRegistry);
    }

    void _commandBuffer_CreateWithComponent_HasComponent(Registry& registry) {
      auto&& [entity, component] = registry.mBuffer.createAndGetEntityWithComponents<TestTypeA>();

      registry.process();

      TestTypeA* value = registry.mRegistry.tryGetComponent<TestTypeA>(entity);
      Assert::IsNotNull(value);
      Assert::AreEqual(*value, *component);
    }

    TEST_METHOD(CommandBuffer_CreateWithComponent_HasComponent) {
      _commandBuffer_CreateWithComponent_HasComponent(*mAllRegistry);
      _commandBuffer_CreateWithComponent_HasComponent(*mSpecificRegistry);
    }

    void _commandBuffer_CreateEmptyThenAddComponent_HasComponent(Registry& registry) {
      auto&& [entity] = registry.mBuffer.createAndGetEntityWithComponents<>();
      TestTypeA& component = registry.mBuffer.addComponent<TestTypeA>(entity);
      component = 7;

      registry.process();

      TestTypeA* result = registry.mRegistry.tryGetComponent<TestTypeA>(entity);
      Assert::IsNotNull(result);
      Assert::AreEqual(component, *result);
    }

    TEST_METHOD(CommandBuffer_CreateEmptyThenAddComponent_HasComponent) {
      _commandBuffer_CreateEmptyThenAddComponent_HasComponent(*mAllRegistry);
      _commandBuffer_CreateEmptyThenAddComponent_HasComponent(*mSpecificRegistry);
    }

    void _commandBuffer_CreateWithComponentThenRemove_DoesntHaveComponent(Registry& registry) {
      auto&& [entity, component] = registry.mBuffer.createAndGetEntityWithComponents<TestTypeA>();
      registry.mBuffer.removeComponent<TestTypeA>(entity);

      registry.process();

      Assert::IsTrue(registry.mRegistry.isValid(entity));
      Assert::IsFalse(registry.mRegistry.hasComponent<TestTypeA>(entity));
    }

    TEST_METHOD(CommandBuffer_CreateWithComponentThenRemove_DoesntHaveComponent) {
      _commandBuffer_CreateWithComponentThenRemove_DoesntHaveComponent(*mAllRegistry);
      _commandBuffer_CreateWithComponentThenRemove_DoesntHaveComponent(*mSpecificRegistry);
    }

    void _commandBuffer_CreateWithOneComponentThenAddAnother_HasBoth(Registry& registry) {
      auto&& [entity, first] = registry.mBuffer.createAndGetEntityWithComponents<TestTypeA>();
      registry.mBuffer.addComponent<TestTypeB>(entity);

      registry.process();

      Assert::IsTrue(registry.mRegistry.hasComponent<TestTypeA>(entity));
      Assert::IsTrue(registry.mRegistry.hasComponent<TestTypeB>(entity));
    }

    TEST_METHOD(CommandBuffer_CreateWithOneComponentThenAddAnother_HasBoth) {
      _commandBuffer_CreateWithOneComponentThenAddAnother_HasBoth(*mAllRegistry);
      _commandBuffer_CreateWithOneComponentThenAddAnother_HasBoth(*mSpecificRegistry);
    }

    void _commandBuffer_CreateEntityRemoveUnrelatedComponent_NothingHappens(Registry& registry) {
      auto&& [entity, a] = registry.mBuffer.createAndGetEntityWithComponents<TestTypeA>();
      registry.mBuffer.removeComponent<TestTypeB>(entity);

      registry.process();

      Assert::IsTrue(registry.mRegistry.hasComponent<TestTypeA>(entity));
    }

    TEST_METHOD(CommandBuffer_CreateEntityRemoveUnrelatedComponent_NothingHappens) {
      _commandBuffer_CreateEntityRemoveUnrelatedComponent_NothingHappens(*mAllRegistry);
      _commandBuffer_CreateEntityRemoveUnrelatedComponent_NothingHappens(*mSpecificRegistry);
    }

    void _commandBuffer_CreateWithComponentThenRemoveAddRemove_IsRemoved(Registry& registry) {
      auto&& [entity, c] = registry.mBuffer.createAndGetEntityWithComponents<TestTypeA>();
      registry.mBuffer.removeComponent<TestTypeA>(entity);
      registry.mBuffer.addComponent<TestTypeA>(entity);
      registry.mBuffer.removeComponent<TestTypeA>(entity);

      registry.process();

      Assert::IsTrue(registry.mRegistry.isValid(entity));
      Assert::IsFalse(registry.mRegistry.hasComponent<TestTypeA>(entity));
    }

    TEST_METHOD(CommandBuffer_CreateWithComponentThenRemoveAddRemove_IsRemoved) {
      _commandBuffer_CreateWithComponentThenRemoveAddRemove_IsRemoved(*mAllRegistry);
      _commandBuffer_CreateWithComponentThenRemoveAddRemove_IsRemoved(*mSpecificRegistry);
    }

    void _commandBuffer_CreateWithComponentProcessTwice_HasOne(Registry& registry) {
      auto&& [entity, c] = registry.mBuffer.createAndGetEntityWithComponents<TestTypeA>();

      registry.process();
      registry.process();

      Assert::IsTrue(registry.mRegistry.hasComponent<TestTypeA>(entity));
      Assert::AreEqual(size_t(1), registry.mRegistry.size<TestTypeA>());
    }

    TEST_METHOD(CommandBuffer_CreateWithComponentProcessTwice_HasOne) {
      _commandBuffer_CreateWithComponentProcessTwice_HasOne(*mAllRegistry);
      _commandBuffer_CreateWithComponentProcessTwice_HasOne(*mSpecificRegistry);
    }

    void _commandBuffer_CreateProcessAddComponent_HasComponent(Registry& registry) {
      auto&& [entity] = registry.mBuffer.createAndGetEntityWithComponents<>();
      registry.process();
      registry.mBuffer.addComponent<TestTypeA>(entity);

      registry.process();

      Assert::IsTrue(registry.mRegistry.hasComponent<TestTypeA>(entity));
    }

    TEST_METHOD(CommandBuffer_CreateProcessAddComponent_HasComponent) {
      _commandBuffer_CreateProcessAddComponent_HasComponent(*mAllRegistry);
      _commandBuffer_CreateProcessAddComponent_HasComponent(*mSpecificRegistry);
    }

    void _commandBuffer_CreateProcessRemoveComponent_IsRemoved(Registry& registry) {
      auto&& [entity, c] = registry.mBuffer.createAndGetEntityWithComponents<TestTypeA>();
      registry.process();
      registry.mBuffer.removeComponent<TestTypeA>(entity);

      registry.process();

      Assert::IsTrue(registry.mRegistry.isValid(entity));
      Assert::IsFalse(registry.mRegistry.hasComponent<TestTypeA>(entity));
    }

    TEST_METHOD(CommandBuffer_CreateProcessRemoveComponent_IsRemoved) {
      _commandBuffer_CreateProcessRemoveComponent_IsRemoved(*mAllRegistry);
      _commandBuffer_CreateProcessRemoveComponent_IsRemoved(*mSpecificRegistry);
    }

    void _commandBuffer_CreateProcessRemove_IsRemoved(Registry& registry) {
      auto&& [entity] = registry.mBuffer.createAndGetEntityWithComponents<>();
      registry.process();
      registry.mBuffer.destroyEntity(entity);

      registry.process();

      Assert::IsFalse(registry.mRegistry.isValid(entity));
    }

    TEST_METHOD(CommandBuffer_CreateProcessRemove_IsRemoved) {
      _commandBuffer_CreateProcessRemove_IsRemoved(*mAllRegistry);
      _commandBuffer_CreateProcessRemove_IsRemoved(*mSpecificRegistry);
    }

    void _commandBuffer_CreateProcessAddProcessRemoveProcessAddProcess_HasComponent(Registry& registry) {
      auto&& [entity] = registry.mBuffer.createAndGetEntityWithComponents<>();
      registry.process();
      registry.mBuffer.addComponent<TestTypeA>(entity);
      registry.process();
      registry.mBuffer.removeComponent<TestTypeA>(entity);
      registry.process();
      registry.mBuffer.addComponent<TestTypeA>(entity);
      registry.process();

      Assert::IsTrue(registry.mRegistry.hasComponent<TestTypeA>(entity));
    }

    TEST_METHOD(CommandBuffer_CreateProcessAddProcessRemoveProcessAddProcess_HasComponent) {
      _commandBuffer_CreateProcessAddProcessRemoveProcessAddProcess_HasComponent(*mAllRegistry);
      _commandBuffer_CreateProcessAddProcessRemoveProcessAddProcess_HasComponent(*mSpecificRegistry);
    }

    TEST_METHOD(CommandBuffer_CreateAddAProcessB_DoesntHaveA) {
      Registry reg;
      auto&& [entity] = reg.mBuffer.createAndGetEntityWithComponents<>();
      TestTypeA value(7);
      reg.mBuffer.addComponent<TestTypeA>(entity) = value;

      reg.processCommandsForComponent<TestTypeB>();

      Assert::IsTrue(reg.mRegistry.isValid(entity));
      TestTypeA* found = reg.mRegistry.tryGetComponent<TestTypeA>(entity);
      Assert::IsNotNull(found, L"Found should exist since the entity is created in the proper chunk, but no value assigned yet");
      Assert::AreEqual(TestTypeA{}, *found, L"Value shouldn't be assigned yet");
    }
  };
}