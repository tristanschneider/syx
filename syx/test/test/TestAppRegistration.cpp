#include "Precompile.h"
#include "test/TestAppRegistration.h"

#include "editor/Editor.h"
#include "component/CameraComponent.h"
#include "component/LuaComponent.h"
#include "component/LuaComponentRegistry.h"
#include "component/NameComponent.h"
#include "component/Physics.h"
#include "component/Renderable.h"
#include "component/SpaceComponent.h"
#include "component/Transform.h"
#include "event/EventBuffer.h"
#include "event/EventHandler.h"
#include "loader/AssetLoader.h"
#include "system/AssetRepo.h"
#include "system/KeyboardInput.h"
#include "system/LuaGameSystem.h"
#include "test/TestListenerSystem.h"

size_t LuaRegistration::TEST_CALLBACK_ID = typeId<TestListenerSystem>();

void LuaRegistration::registerSystems(const SystemArgs& args, ISystemRegistry& registry) {
  auto assetLoaders = Registry::createAssetLoaderRegistry();
  assetLoaders->registerLoader<TextAsset, TextAssetLoader>("txt");

  registry.registerSystem(std::make_unique<AssetRepo>(args, std::move(assetLoaders)));
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

namespace TestRegistration {
  std::unique_ptr<AppRegistration> createEditorRegistration() {
    struct Reg : public LuaRegistration {
      void registerSystems(const SystemArgs& args, ISystemRegistry& registry) override {
        LuaRegistration::registerSystems(args, registry);
        registry.registerSystem(std::make_unique<Editor>(args));
        registry.registerSystem(std::make_unique<KeyboardInput>(args));
      }

      void registerComponents(IComponentRegistry& registry) override {
        LuaRegistration::registerComponents(registry);
      }
    };

    return std::make_unique<Reg>();
  }
}