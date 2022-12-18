#include "Precompile.h"
#include "CppUnitTest.h"

#include "ecs/component/GameobjectComponent.h"
#include "ecs/component/PhysicsComponents.h"
#include "ecs/component/SpaceComponents.h"
#include "test/TestAppContext.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace SystemTests {
  TEST_CLASS(GameObjectInitializerSystemTests) {
    TEST_METHOD(GameobjectInitializer_CreateEmptyObject_ComponentsAdded) {
      TestAppContext app;
      auto entity = app.mRegistry.createEntityWithComponents<GameobjectComponent>();

      app.update();

      Assert::IsNotNull(app.mRegistry.tryGetComponent<GameobjectInitializedComponent>(entity));
      Assert::IsNotNull(app.mRegistry.tryGetComponent<NameTagComponent>(entity));
      Assert::IsNotNull(app.mRegistry.tryGetComponent<SerializeIDComponent>(entity));
      InSpaceComponent* inSpace = app.mRegistry.tryGetComponent<InSpaceComponent>(entity);
      Assert::IsNotNull(inSpace);
      Assert::IsNotNull(app.mRegistry.tryGetComponent<DefaultPlaySpaceComponent>(inSpace->mSpace));
    }

    TEST_METHOD(PhysicsOwnerWithDestructionRequest_Tick_OwnerAndPhysicsObjectDestroyed) {
      TestAppContext app;
      auto entity = app.mRegistry.createEntityWithComponents<CreatePhysicsObjectRequestComponent>();
      app.update();
      PhysicsOwner* owner = app.mRegistry.tryGetComponent<PhysicsOwner>(entity);
      Assert::IsNotNull(owner);
      Engine::Entity physicsObject = owner->mPhysicsObject;
      app.mRegistry.addComponent<DestroyGameobjectComponent>(entity);

      //One to see the destruction request, one to unlink the physics object
      app.update();
      app.update();

      Assert::IsFalse(app.mRegistry.isValid(entity));
      Assert::IsFalse(app.mRegistry.isValid(physicsObject));
    }
  };
}