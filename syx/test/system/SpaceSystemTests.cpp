#include "Precompile.h"
#include "CppUnitTest.h"

#include "ecs/component/FileSystemComponent.h"
#include "ecs/component/MessageComponent.h"
#include "ecs/component/SpaceComponents.h"
#include "ecs/component/TransformComponent.h"
#include "ecs/system/LuaSpaceSerializerSystem.h"
#include "ecs/system/RemoveEntitiesSystem.h"
#include "ecs/system/SpaceSystem.h"
#include "SystemRegistry.h"
#include "TypeInfo.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace SystemTests {
  using namespace Engine;

  TEST_CLASS(SpaceSystemTests) {
    Entity addEntity(EntityRegistry& registry, Entity space) {
      auto entity = registry.createEntity();
      registry.addComponent<TransformComponent>(entity);
      registry.addComponent<InSpaceComponent>(entity, space);
      return entity;
    }

    ecx::SystemRegistry<Entity> createTestSystems() {
      ecx::SystemRegistry<Entity> systems;
      using SystemT = ComponentSerializeSystem<TransformComponent, LuaComponentSerialize<TransformComponent>>;
      systems.registerSystem(SpaceSystem::clearSpaceSystem());
      systems.registerSystem(SpaceSystem::beginLoadSpaceSystem());
      systems.registerSystem(SpaceSystem::parseSceneSystem());
      systems.registerSystem(SpaceSystem::createSpaceEntitiesSystem());
      systems.registerSystem(SystemT::createDeserializer());
      systems.registerSystem(SpaceSystem::completeSpaceLoadSystem());

      systems.registerSystem(SpaceSystem::beginSaveSpaceSystem());
      systems.registerSystem(SpaceSystem::createSerializedEntitiesSystem());
      systems.registerSystem(SystemT::createSerializer());
      systems.registerSystem(SpaceSystem::serializeSpaceSystem());
      systems.registerSystem(SpaceSystem::completeSpaceSaveSystem());

      systems.registerSystem(RemoveEntitiesSystem<View<Read<MessageComponent>>>::create());
      return systems;
    }

    void clearMessages(EntityRegistry& registry) {
      RemoveEntitiesSystem<View<Read<MessageComponent>>>::create()->tick(registry);
    }

    TEST_METHOD(ClearSpaceSystem_Tick_ClearsEntitiesInSpace) {
      EntityRegistry registry;
      Entity space = registry.createEntityWithComponents<SpaceTagComponent>();
      Entity otherSpace = registry.createEntityWithComponents<SpaceTagComponent>();
      Entity inSpace = addEntity(registry, space);
      Entity notInSpace = addEntity(registry, otherSpace);
      Entity message = registry.createEntityWithComponents<MessageComponent, ClearSpaceComponent>();
      registry.getComponent<ClearSpaceComponent>(message).mSpace = space;

      SpaceSystem::clearSpaceSystem()->tick(registry);

      Assert::AreEqual(size_t(1), registry.size<InSpaceComponent>(), L"Only the entity in the other space should remain", LINE_INFO());
    }

    TEST_METHOD(SerializeSystems_Tick_CreatesFileWriteRequest) {
      EntityRegistry registry;
      FilePath savePath("testpath");
      Entity space = registry.createEntityWithComponents<SpaceTagComponent>();
      Entity inSpace = addEntity(registry, space);
      Entity message = registry.createEntity();
      registry.addComponent<SaveSpaceComponent>(message, space, savePath);

      createTestSystems().tick(registry);

      Assert::AreEqual(size_t(1), registry.size<FileWriteRequest>(), L"Space serialization should have triggered a file write request", LINE_INFO());
      Assert::IsTrue(savePath == registry.begin<FileWriteRequest>().component().mToWrite);
    }

    TEST_METHOD(DeserializeSystems_Tick_CreatesFileReadRequest) {
      EntityRegistry registry;
      FilePath path("testpath");
      Entity space = registry.createEntityWithComponents<SpaceTagComponent>();
      Entity message = registry.createEntity();
      registry.addComponent<LoadSpaceComponent>(message, space, path);

      createTestSystems().tick(registry);

      Assert::AreEqual(size_t(1), registry.size<FileReadRequest>(), L"Space serialization should have triggered a file write request", LINE_INFO());
      Assert::IsTrue(path == registry.begin<FileReadRequest>().component().mToRead);
    }

    void doRoundTrip(EntityRegistry& registry, Entity space) {
      FilePath savePath("testpath");
      Entity message = registry.createEntityWithComponents<MessageComponent>();
      registry.addComponent<SaveSpaceComponent>(message, space, savePath);

      //Save
      createTestSystems().tick(registry);
      //Grab the written contents to echo back in the read request
      auto saveRequest = registry.begin<FileWriteRequest>();
      Assert::IsTrue(saveRequest != registry.end<FileWriteRequest>());
      FileReadSuccessResponse simulatedRead{ std::move(saveRequest->mBuffer) };
      //Simulate successful write so completeSpaceWriteSystem clears components
      registry.addComponent<FileWriteSuccessResponse>(space);
      //Clear the file write
      createTestSystems().tick(registry);

      //Clear the space before loading
      message = registry.createEntityWithComponents<MessageComponent>();
      registry.addComponent<ClearSpaceComponent>(message, space);
      //Load the new space
      message = registry.createEntityWithComponents<MessageComponent>();
      registry.addComponent<LoadSpaceComponent>(message, space, savePath);
      //Tick should begin the load and request the file read
      createTestSystems().tick(registry);

      //Echo written contents in a read response
      auto readRequest = registry.begin<FileReadRequest>();
      Assert::IsTrue(readRequest != registry.end<FileReadRequest>());
      registry.addComponent<FileReadSuccessResponse>(readRequest.entity(), std::move(simulatedRead));
      //Tick should process the read success response
      createTestSystems().tick(registry);
    }

    TEST_METHOD(SerializeRoundTrip_Tick_MatchesOriginal) {
      EntityRegistry registry;
      Entity space = registry.createEntityWithComponents<SpaceTagComponent>();
      Entity inSpace = addEntity(registry, space);
      registry.getComponent<TransformComponent>(inSpace).mValue[0][0] = 7.f;

      doRoundTrip(registry, space);

      Assert::AreEqual(size_t(1), registry.size<TransformComponent>(), L"Transform should have been restored by load", LINE_INFO());
      Assert::AreEqual(size_t(1), registry.size<InSpaceComponent>(), L"Restored entity should have in space component", LINE_INFO());
      Assert::AreEqual(7.f, registry.begin<TransformComponent>()->mValue[0][0], L"Transform value should have been restored", LINE_INFO());
      Assert::IsTrue(space == registry.begin<InSpaceComponent>()->mSpace, L"Restored entity should be in the load space", LINE_INFO());
    }

    TEST_METHOD(SerializeRoundTrip_TwoTicks_MatchesOriginal) {
      EntityRegistry registry;
      Entity space = registry.createEntityWithComponents<SpaceTagComponent>();
      Entity inSpace = addEntity(registry, space);
      registry.getComponent<TransformComponent>(inSpace).mValue[0][0] = 7.f;

      doRoundTrip(registry, space);
      doRoundTrip(registry, space);

      Assert::AreEqual(size_t(1), registry.size<TransformComponent>(), L"Transform should have been restored by load", LINE_INFO());
      Assert::AreEqual(size_t(1), registry.size<InSpaceComponent>(), L"Restored entity should have in space component", LINE_INFO());
      Assert::AreEqual(7.f, registry.begin<TransformComponent>()->mValue[0][0], L"Transform value should have been restored", LINE_INFO());
      Assert::IsTrue(space == registry.begin<InSpaceComponent>()->mSpace, L"Restored entity should be in the load space", LINE_INFO());
    }

    TEST_METHOD(SerializeRoundTripTwoEntities_TwoTicks_AllRestored) {
      EntityRegistry registry;
      Entity space = registry.createEntityWithComponents<SpaceTagComponent>();
      addEntity(registry, space);
      addEntity(registry, space);

      doRoundTrip(registry, space);
      doRoundTrip(registry, space);

      Assert::AreEqual(size_t(2), registry.size<TransformComponent>(), L"Transform should have been restored by load", LINE_INFO());
      Assert::AreEqual(size_t(2), registry.size<InSpaceComponent>(), L"Restored entity should have in space component", LINE_INFO());
    }

    //TODO: test with AppRegistration and TestFileSystem
    //TODO: test failure cases
  };
}