#include "Precompile.h"
#include "CppUnitTest.h"

#include "ecs/system/LuaSpaceSerializerSystem.h"

#include "ecs/component/TransformComponent.h"
#include "SystemRegistry.h"
#include "TypeInfo.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace SystemTests {
  using namespace Engine;

  TEST_CLASS(SpaceSerializerSystemTests) {
    Entity createSavingSpace(EntityRegistry& registry, ParsedSpaceContentsComponent parsed) {
      auto space = registry.createEntity();
      registry.addComponent<SpaceSavingComponent>(space);
      registry.addComponent<SpaceFillingEntitiesComponent>(space);
      registry.addComponent<ParsedSpaceContentsComponent>(space) = std::move(parsed);
      return space;
    }

    Entity createLoadingSpace(EntityRegistry& registry, ParsedSpaceContentsComponent parsed) {
      auto space = registry.createEntity();
      registry.addComponent<SpaceLoadingComponent>(space);
      registry.addComponent<SpaceFillingEntitiesComponent>(space);
      registry.addComponent<ParsedSpaceContentsComponent>(space) = std::move(parsed);
      return space;
    }

    Entity addEntity(EntityRegistry& registry) {
      auto entity = registry.createEntity();
      registry.addComponent<TransformComponent>(entity);
      return entity;
    }

    ecx::SystemRegistry<Entity> createTestSystems() {
      ecx::SystemRegistry<Entity> systems;
      using SystemT = ComponentSerializeSystem<TransformComponent, LuaComponentSerialize<TransformComponent>>;
      systems.registerSystem(SystemT::createSerializer());
      systems.registerSystem(SystemT::createDeserializer());
      return systems;
    }

    TEST_METHOD(EmptySpaceSerialize_Tick_NothingHappens) {
      EntityRegistry registry;
      auto space = createSavingSpace(registry, {});

      createTestSystems().tick(registry);
    }

    TEST_METHOD(EmptySpaceDeserialize_Tick_NothingHappens) {
      EntityRegistry registry;
      auto space = createLoadingSpace(registry, {});

      createTestSystems().tick(registry);
    }

    TEST_METHOD(SingleEntitySerialize_Tick_IsSerialized) {
      EntityRegistry registry;
      ParsedSpaceContentsComponent contents;
      contents.mNewEntities.push_back(addEntity(registry));
      auto space = createSavingSpace(registry, contents);

      createTestSystems().tick(registry);

      Assert::AreEqual(size_t(1), registry.getComponent<ParsedSpaceContentsComponent>(space).mSections.size());
    }

    TEST_METHOD(TwoEntitySerialize_Tick_AreSerialized) {
      EntityRegistry registry;
      ParsedSpaceContentsComponent contents;
      contents.mNewEntities.push_back(addEntity(registry));
      contents.mNewEntities.push_back(addEntity(registry));
      auto space = createSavingSpace(registry, contents);

      createTestSystems().tick(registry);

      Assert::AreEqual(size_t(1), registry.getComponent<ParsedSpaceContentsComponent>(space).mSections.size());
    }

    TEST_METHOD(SingleEntity_TickRoundTrip_IsSame) {
      EntityRegistry registry;
      ParsedSpaceContentsComponent contents;
      auto entity = addEntity(registry);
      registry.getComponent<TransformComponent>(entity).mValue[0][0] = 3.f;
      contents.mNewEntities.push_back(entity);
      auto space = createSavingSpace(registry, contents);

      //Save
      createTestSystems().tick(registry);
      //Remove the serialized component so we can test if it came back
      registry.removeComponent<TransformComponent>(entity);
      //Transfer the saved components to a load request in a new space
      auto loadContents = registry.getComponent<ParsedSpaceContentsComponent>(space);
      auto loadSpace = createLoadingSpace(registry, std::move(loadContents));
      //Load
      createTestSystems().tick(registry);

      const TransformComponent* transform = registry.tryGetComponent<TransformComponent>(entity);
      Assert::IsNotNull(transform, L"Component should have been added by deserialization system", LINE_INFO());
      Assert::AreEqual(3.f, transform->mValue[0][0], L"Value should be preserved", LINE_INFO());
    }
  };
}