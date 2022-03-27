#include "Precompile.h"
#include "CppUnitTest.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;

#include "App.h"
#include "AppPlatform.h"
#include "AppRegistration.h"
#include "component/CameraComponent.h"
#include "component/LuaComponent.h"
#include "component/LuaComponentRegistry.h"
#include "component/NameComponent.h"
#include "component/Physics.h"
#include "component/Renderable.h"
#include "component/SpaceComponent.h"
#include "component/Transform.h"
#include "DebugDrawer.h"
#include "event/DebugDrawEvent.h"
#include "file/FilePath.h"
#include "file/DirectoryWatcher.h"
#include "system/AssetRepo.h"
#include "system/LuaGameSystem.h"
#include "system/PhysicsSystem.h"
#include "SyxVec2.h"
#include "asset/LuaScript.h"
#include "event/BaseComponentEvents.h"
#include "event/SpaceEvents.h"
#include "event/EventBuffer.h"
#include "LuaGameObject.h"
#include "lua/LuaGameContext.h"
#include "lua/LuaState.h"
#include "lua/LuaVariant.h"
#include "test/MockApp.h"

#include "test/TestAppPlatform.h"
#include "test/TestAppRegistration.h"

#include "SyxInterface.h"

#include "event/DeferredEventBuffer.h"

namespace Syx {
  namespace Interface {
    extern ::MessageQueueProvider* gMsg;
    extern SyxOptions gOptions;
  }
}

namespace SystemTests {
  struct MockDebugDrawer : public MessageQueueProvider {
    struct Command {
      enum class Type : uint8_t {
        Line,
        Vector,
        Sphere,
        Cube,
        Point,
        SetColor,
      };

      Type mType;
      Syx::Vec3 mA;
    };

    MockDebugDrawer() {
      Syx::Interface::gMsg = this;
    }

    ~MockDebugDrawer() {
      Syx::Interface::gMsg = nullptr;
    }

    MessageQueue getMessageQueue() override {
      return MessageQueue(mBuffer, mLock);
    }

    //Provide dummy objects, this isn't used by the physics system
    DeferredMessageQueue getDeferredMessageQueue() override {
      static DeferredEventBuffer dummy;
      static std::mutex mutex;
      return DeferredMessageQueue(dummy, mutex);
    }

    template<class E>
    const E* getLastEventOfType() const {
      static_assert(std::is_base_of_v<Event, E>, "This should be called with event types");
      const E* result = nullptr;
      for(auto&& e : mBuffer) {
        if(e.getType() == typeId<E, Event>()) {
          result = static_cast<const E*>(&e);
        }
      }
      return result;
    }

    SpinLock mLock;
    EventBuffer mBuffer;
  };

  TEST_CLASS(PhysicsSystemTests) {
  public:
    std::unique_ptr<ILuaGameContext> _createGameContext(App& app) {
      return Lua::createGameContext(*app.getSystem<LuaGameSystem>());
    }

    MockApp _createApp() {
      MockApp app(std::make_unique<TestAppPlatform>(), TestRegistration::createPhysicsRegistration());
      app->getMessageQueue()->push(SetTimescaleEvent(0, 1.f));
      return app;
    }

    struct GameObjectBuilder {
      GameObjectBuilder(ILuaGameContext& context)
        : mContext(&context) {
      }
      GameObjectBuilder(GameObjectBuilder&) = default;

      GameObjectBuilder& createGameObject() {
        mCurrentObj = &mContext->addGameObject();
        return *this;
      }

      GameObjectBuilder& setTransform(const Syx::Mat4& transform) {
        if(mCurrentObj) {
          Transform newTransform(mCurrentObj->getRuntimeID());
          newTransform.set(transform);
          mCurrentObj->getComponent(newTransform.getFullType())->set(newTransform);
        }
        return *this;
      }

      GameObjectBuilder& addPhysicsComponent() {
        if(mCurrentObj) {
          mCurrentObj->addComponent("Physics");
        }
        return *this;
      }

      GameObjectBuilder& enableCollider(const std::string& modelName) {
        if(mCurrentObj) {
          if(IComponent* component = mCurrentObj->getComponent(Physics::singleton().getFullType())) {
            Physics physics = component->get<Physics>();
            PhysicsData data = physics.getData();
            data.mHasCollider = true;
            AssetInfo cubeInfo(modelName);
            cubeInfo.fill();
            data.mModel = cubeInfo.mId;

            physics.setData(data);
            component->set(physics);
          }
        }
        return *this;
      }

      GameObjectBuilder& enableRigidbody() {
        if(mCurrentObj) {
          if(IComponent* component = mCurrentObj->getComponent(Physics::singleton().getFullType())) {
            Physics physics = component->get<Physics>();
            PhysicsData data = physics.getData();
            data.mHasRigidbody = true;

            physics.setData(data);
            component->set(physics);
          }
        }
        return *this;
      }

      GameObjectBuilder& refreshComponents() {
        const Handle h = mCurrentObj ? mCurrentObj->getRuntimeID() : InvalidHandle;
        mContext->clearCache();
        mCurrentObj = mContext->getGameObject(h);
        return *this;
      }

      ILuaGameContext* mContext = nullptr;
      IGameObject* mCurrentObj = nullptr;
    };

    TEST_METHOD(EmptyScene_Update_NothingHappens) {
      _createApp()->update(1.f);
    }
    /* TODO: fix or delete
    TEST_METHOD(SingleObject_AddPhysicsComponent_PositionsMatch) {
      auto app = _createApp();
      auto context = _createGameContext(*app);
      auto builder = GameObjectBuilder(*context)
        .createGameObject()
        //Set at a nonzero position
        .setTransform(Syx::Mat4::transform(Syx::Quat::Zero, Syx::Vec3(1, 2, 3)))
        .addPhysicsComponent()
        .enableCollider(PhysicsSystem::CUBE_MODEL_NAME)
        .enableRigidbody();
      MockDebugDrawer debugDrawer;

      //Create the object and send the transform data request
      app->update(0.f);
      //Respond to the transform data request
      app->update(0.f);
      //Process the transform data response
      app->update(0.f);

      auto e = debugDrawer.getLastEventOfType<DrawPointEvent>();
      Assert::IsNotNull(e);
      Assert::IsTrue(e->mPoint.distance2(Syx::Vec3(1, 2, 3)) < 0.001f, L"Position of physics object should match transform of gameobject", LINE_INFO());
    }

    TEST_METHOD(RigidbodyWithCollider_Update_GravityApplied) {
      auto app = _createApp();
      auto context = _createGameContext(*app);
      auto builder = GameObjectBuilder(*context)
        .createGameObject()
        .addPhysicsComponent()
        .enableRigidbody()
        .enableCollider(PhysicsSystem::CUBE_MODEL_NAME);
      MockDebugDrawer debugDrawer;

      app->update(1.f);

      auto e = debugDrawer.getLastEventOfType<DrawPointEvent>();
      Assert::IsNotNull(e);
      Assert::IsTrue(e->mPoint.y < 0, L"Gravity should have moved the rigidbody below its original position of 0", LINE_INFO());
    }
    */

    /* TODO: fails in release sometimes
    TEST_METHOD(GameobjectWithRigidbody_Update_GravityApplied) {
      auto app = _createApp();
      auto context = _createGameContext(*app);
      auto builder = GameObjectBuilder(*context)
        .createGameObject()
        .addPhysicsComponent()
        .enableRigidbody();
      const Syx::Vec3 originalPos = builder.mCurrentObj->getComponent(Transform::singleton().getFullType())->get<Transform>().get().getTranslate();

      //Physics system will broadcast new positions at the end of this update
      app->update(1.f);
      //Results will be processed by gameplay on this update
      app->update(0.f);

      builder.refreshComponents();
      const Syx::Vec3 newPos = builder.mCurrentObj->getComponent(Transform::singleton().getFullType())->get<Transform>().get().getTranslate();
      Assert::IsTrue(newPos.y < originalPos.y, L"Gravity should have moved the rigidbody below its original position", LINE_INFO());
    }
    */

    //TODO:
    //Test to prove that applying forces doesn't cancel out physics updates
    //GameObjectWithRigidbody_ApplyForce_ObjectMoves
    //GameObjectWithRigidbody_ApplyForce_GravityUnaffected
    //SleepingRigidbody_ApplyForce_ObjectMoves
  };
}