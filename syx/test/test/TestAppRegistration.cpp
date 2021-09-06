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
#include "system/PhysicsSystem.h"
#include "test/TestListenerSystem.h"

typeId_t<System> LuaRegistration::TEST_CALLBACK_ID = typeId<TestListenerSystem, System>();

//TODO: move to its own file
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "ImGuiImpl.h"
#include "system/ImGuiSystem.h"
#include "util/ScratchPad.h"

//Imgui but without any graphics calls
struct TestImGuiSystem : public IImGuiSystem {
  using IImGuiSystem::IImGuiSystem;

  void init() override {
    ImGuiIO& io = ImGui::GetIO();
    //Prevent saving anyything to disk during tests
    io.IniFilename = nullptr;
    //Arbitrary values, just needed to be able to accept commands
    io.DisplaySize = ImVec2(100.f, 100.f);
    io.DisplayFramebufferScale = ImVec2(1.f, 1.f);
    io.DeltaTime = 0.01f;
    unsigned char* pixels;
    int width, height;
    //Don't care about font but need to call this so it doesn't assert about being uninitialized
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    ImGui::NewFrame();
  }

  void queueTasks(float, IWorkerPool&, std::shared_ptr<Task>) override {
    IImGuiImpl::getPad().update();
    ImGui::EndFrame();
    ImGui::NewFrame();
  }

  void uninit() override {
    ImGui::Shutdown();
  }

  IImGuiImpl* _getImpl() override {
    return nullptr;
  }
};

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
        registry.registerSystem(std::make_unique<TestImGuiSystem>(args));
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

  std::unique_ptr<AppRegistration> createPhysicsRegistration() {
    struct Reg : public LuaRegistration {
      void registerSystems(const SystemArgs& args, ISystemRegistry& registry) override {
        LuaRegistration::registerSystems(args, registry);
        registry.registerSystem(std::make_unique<PhysicsSystem>(args));
      }
    };

    return std::make_unique<Reg>();
  }
}