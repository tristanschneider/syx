#include "Precompile.h"
#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

#include "ecs/component/PhysicsComponents.h"
#include "ecs/component/TransformComponent.h"
#include "ecs/system/physics/PhysicsSystem.h"
#include "SyxQuat.h"
#include "test/TestAppContext.h"

namespace SystemTests {
  using namespace Engine;
  TEST_CLASS(PhysicsSystemTests) {
    struct PhysicsPair {
      Entity mOwner, mPhysicsObject;
    };

    static void tickPhysicsSystems(EntityRegistry& registry) {
      for(auto&& system : PhysicsSystems::createDefault()) {
        system->tick(registry);
      }
    }

    static PhysicsPair createOwnedPhysicsObject(TestAppContext& app, Entity entity) {
      PhysicsPair result;
      result.mOwner = entity;

      app.update();

      PhysicsOwner* owner = app.mRegistry.tryGetComponent<PhysicsOwner>(result.mOwner);
      Assert::IsNotNull(owner);
      result.mPhysicsObject = owner->mPhysicsObject;
      Assert::IsNotNull(app.mRegistry.tryGetComponent<PhysicsObject>(result.mPhysicsObject));

      return result;
    }

    static PhysicsPair createPhysicsPair(TestAppContext& app) {
      return createOwnedPhysicsObject(app, app.mRegistry.createEntityWithComponents<CreatePhysicsObjectRequestComponent, TransformComponent>());
    }

    static PhysicsPair createPhysicsPairSphere(TestAppContext& app, float radius, float density) {
      auto result = app.mRegistry.createAndGetEntityWithComponents<CreatePhysicsObjectRequestComponent, TransformComponent, PhysicsModelComponent>();
      PhysicsModelComponent& model = std::get<3>(result).get();
      model.mModel = PhysicsModelComponent::Sphere{ radius };
      model.mDensity = density;
      return createOwnedPhysicsObject(app, std::get<0>(result));
    }

    TEST_METHOD(PhysicsOwnerWithCreateRequest_Tick_PhysicsObjectCreated) {
      TestAppContext app;
      PhysicsPair pair = createPhysicsPair(app);

      PhysicsObject* obj = app.mRegistry.tryGetComponent<PhysicsObject>(pair.mPhysicsObject);
      PhysicsOwner* owner = app.mRegistry.tryGetComponent<PhysicsOwner>(pair.mOwner);
      Assert::IsNotNull(obj);
      Assert::IsNotNull(owner);
      Assert::IsTrue(obj->mPhysicsOwner == pair.mOwner);
      Assert::IsTrue(owner->mPhysicsObject == pair.mPhysicsObject);
      Assert::IsFalse(app.mRegistry.hasComponent<CreatePhysicsObjectRequestComponent>(pair.mOwner));
    }

    TEST_METHOD(PhysicsOwnerWithTransformAndCreate_Tick_PhysicsObjectCreatedAtPosition) {
      TestAppContext app;
      auto& reg = app.mRegistry;
      auto&& [ entity, req, transform ] = reg.createAndGetEntityWithComponents<CreatePhysicsObjectRequestComponent, TransformComponent>();
      Syx::Quat rot = Syx::Quat::axisAngle(Syx::Vec3(1, 0, 0), 2.9f);
      Syx::Vec3 pos(1, 2, 3);
      transform.get().mValue = Syx::Mat4::transform(rot, pos);
      PhysicsPair pair = createOwnedPhysicsObject(app, entity);

      using namespace Tags;
      Assert::AreEqual(pos.x, reg.getComponent<FloatComponent<Position, X>>(pair.mPhysicsObject).mValue);
      Assert::AreEqual(pos.y, reg.getComponent<FloatComponent<Position, Y>>(pair.mPhysicsObject).mValue);
      Assert::AreEqual(pos.z, reg.getComponent<FloatComponent<Position, Z>>(pair.mPhysicsObject).mValue);
      Assert::AreEqual(rot.mV.x, reg.getComponent<FloatComponent<Rotation, I>>(pair.mPhysicsObject).mValue, 0.001f);
      Assert::AreEqual(rot.mV.y, reg.getComponent<FloatComponent<Rotation, J>>(pair.mPhysicsObject).mValue, 0.001f);
      Assert::AreEqual(rot.mV.z, reg.getComponent<FloatComponent<Rotation, K>>(pair.mPhysicsObject).mValue, 0.001f);
      Assert::AreEqual(rot.mV.w, reg.getComponent<FloatComponent<Rotation, W>>(pair.mPhysicsObject).mValue, 0.001f);
    }

    TEST_METHOD(PhysicsOwnerWithDestroyRequest_Tick_PhysicsObjectDestroyed) {
      TestAppContext app;
      auto& reg = app.mRegistry;
      PhysicsPair pair = createPhysicsPair(app);
      reg.addComponent<DestroyPhysicsObjectRequestComponent>(pair.mOwner);

      app.update();

      Assert::IsFalse(reg.isValid(pair.mPhysicsObject));
      Assert::IsTrue(reg.isValid(pair.mOwner));
      Assert::IsFalse(reg.hasComponent<PhysicsOwner>(pair.mOwner));
      Assert::IsFalse(reg.hasComponent<DestroyPhysicsObjectRequestComponent>(pair.mOwner));
    }

    TEST_METHOD(PhysicsOwnerWithSphereModelToSync_Tick_IsSynced) {
      TestAppContext app;
      auto& reg = app.mRegistry;
      PhysicsPair pair = createPhysicsPair(app);
      reg.addComponent<PhysicsModelComponent>(pair.mOwner).mModel = PhysicsModelComponent::Sphere{ 2.0f };
      reg.addComponent<SyncModelRequestComponent>(pair.mPhysicsObject);

      app.update();

      Assert::AreEqual(2.0f, reg.getComponent<FloatComponent<Tags::SphereModel, Tags::Radius>>(pair.mPhysicsObject).mValue);
      Assert::IsFalse(reg.hasComponent<SyncModelRequestComponent>(pair.mOwner));
    }

    TEST_METHOD(PhysicsOwnerWithDifferentSphereModelToSync_Tick_IsSynced) {
      TestAppContext app;
      auto& reg = app.mRegistry;
      PhysicsPair pair = createPhysicsPair(app);
      reg.addComponent<PhysicsModelComponent>(pair.mOwner);
      //Sync once default constructed
      reg.addComponent<SyncModelRequestComponent>(pair.mPhysicsObject);
      app.update();

      //Sync again with different value
      reg.getComponent<PhysicsModelComponent>(pair.mOwner).mModel = PhysicsModelComponent::Sphere{ 2.0f };
      reg.addComponent<SyncModelRequestComponent>(pair.mPhysicsObject);
      app.update();

      Assert::AreEqual(2.0f, reg.getComponent<FloatComponent<Tags::SphereModel, Tags::Radius>>(pair.mPhysicsObject).mValue);
      Assert::IsFalse(reg.hasComponent<SyncModelRequestComponent>(pair.mOwner));
    }

    TEST_METHOD(PhysicsOwnerSphere_Tick_HassMass) {
      TestAppContext app;
      auto& reg = app.mRegistry;
      const float radius = 2.0f;
      const float density = 3.0f;
      const float pi = 3.14159265358979323846f;
      const float expectedMass = (4.0f*pi/3.0f)*radius*radius*radius*density;
      const float expectedInertia = (2.0f/5.0f)*radius*radius*expectedMass;

      PhysicsPair pair = createPhysicsPairSphere(app, radius, density);

      using namespace Tags;
      constexpr float e = 0.00001f;
      Assert::AreEqual(1.0f/expectedMass, reg.getComponent<FloatComponent<Mass, Value>>(pair.mPhysicsObject).mValue, e);
      Assert::AreEqual(1.0f/expectedInertia, reg.getComponent<FloatComponent<LocalInertia, X>>(pair.mPhysicsObject).mValue, e);
      Assert::AreEqual(1.0f/expectedInertia, reg.getComponent<FloatComponent<LocalInertia, Y>>(pair.mPhysicsObject).mValue, e);
      Assert::AreEqual(1.0f/expectedInertia, reg.getComponent<FloatComponent<LocalInertia, Z>>(pair.mPhysicsObject).mValue, e);
    }
  };
}