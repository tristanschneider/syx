#include "Precompile.h"
#include "test/TestAppRegistration.h"

#include "component/CameraComponent.h"
#include "component/LuaComponent.h"
#include "component/LuaComponentRegistry.h"
#include "component/NameComponent.h"
#include "component/Physics.h"
#include "component/Renderable.h"
#include "component/SpaceComponent.h"
#include "component/Transform.h"

#include "system/AssetRepo.h"
#include "system/LuaGameSystem.h"

#include "event/EventBuffer.h"
#include "event/EventHandler.h"

namespace {
  //A system to allow tests to get responses to RequestEvent types via CallbackEvent
  struct TestListenerSystem : public System {
    TestListenerSystem(const SystemArgs& args)
      : System(args, _typeId<TestListenerSystem>()) {
    }

    void init() override {
      mEventHandler = std::make_unique<EventHandler>();
      mEventHandler->registerEventHandler(CallbackEvent::getHandler(LuaRegistration::TEST_CALLBACK_ID));
    }

    void update(float, IWorkerPool&, std::shared_ptr<Task>) override {
      mEventHandler->handleEvents(*mEventBuffer);
    }
  };
}

size_t LuaRegistration::TEST_CALLBACK_ID = typeId<TestListenerSystem>();

void LuaRegistration::registerSystems(const SystemArgs& args, ISystemRegistry& registry) {
  registry.registerSystem(std::make_unique<AssetRepo>(args, Registry::createAssetLoaderRegistry()));
  registry.registerSystem(std::make_unique<LuaGameSystem>(args));
  registry.registerSystem(std::make_unique<TestListenerSystem>(args));
}

void LuaRegistration::registerComponents(IComponentRegistry& registry) {
  registry.registerComponent<CameraComponent>();
  registry.registerComponent<LuaComponent>();
  registry.registerComponent<NameComponent>();
  registry.registerComponent<Physics>();
  registry.registerComponent<Renderable>();
  registry.registerComponent<SpaceComponent>();
  registry.registerComponent<Transform>();
}