#include "Precompile.h"
#include "AppRegistration.h"

#include "asset/LuaScript.h"
#include "asset/Model.h"
#include "asset/PhysicsModel.h"
#include "asset/Shader.h"
#include "asset/Texture.h"
#include "ecs/system/EditorSystem.h"
#include "ecs/component/MessageComponent.h"
#include "ecs/component/TransformComponent.h"
#include "ecs/system/DeltaTimeSystem.h"
#include "ecs/system/FileSystemSystem.h"
#include "ecs/system/GraphicsSystemBase.h"
#include "ecs/system/LuaSpaceSerializerSystem.h"
#include "ecs/system/ProjectLocatorSystem.h"
#include "ecs/system/RawInputSystem.h"
#include "ecs/system/RemoveEntitiesSystem.h"
#include "ecs/system/SpaceSerializerSystem.h"
#include "ecs/system/SpaceSystem.h"
#include "editor/Editor.h"
#include "component/CameraComponent.h"
#include "component/LuaComponent.h"
#include "component/LuaComponentRegistry.h"
#include "component/NameComponent.h"
#include "component/Physics.h"
#include "component/Renderable.h"
#include "component/SpaceComponent.h"
#include "component/Transform.h"
#include "loader/AssetLoader.h"
#include "loader/LuaScriptLoader.h"
#include "loader/ModelLoader.h"
#include "loader/ShaderLoader.h"
#include "loader/TextureLoader.h"
#include "system/AssetRepo.h"
#include "system/GraphicsSystem.h"
#include "system/ImGuiSystem.h"
#include "system/LuaGameSystem.h"
#include "system/PhysicsSystem.h"

class DefaultAppRegistration : public AppRegistration {
public:
  void registerAppContext(Engine::AppContext& context) override {
    using namespace Engine;
    SystemList input, simulation, graphics, physics, cleanup, initializers;

    initializers.push_back(ProjectLocatorSystem::init());
    initializers.push_back(FileSystemSystem::addFileSystemComponent(FileSystem::createStd()));
    initializers.push_back(DeltaTimeSystem::init());
    initializers.push_back(RawInputSystem::init());
    //TODO: only if editor features should be enabled
    initializers.push_back(EditorSystem::init());
    initializers.push_back(GraphicsSystemBase::init());

    //DT update a bit arbitrary, uses input since it's the first 60fps system group
    input.push_back(DeltaTimeSystem::update());
    input.push_back(RawInputSystem::update());

    simulation.push_back(ProjectLocatorSystem::createUriListener());
    simulation.push_back(EditorSystem::createUriListener());

    using TransformT = ComponentSerializeSystem<TransformComponent, LuaComponentSerialize<TransformComponent>>;
    //Load space
    simulation.push_back(SpaceSystem::clearSpaceSystem());
    simulation.push_back(SpaceSystem::beginLoadSpaceSystem());
    simulation.push_back(SpaceSystem::parseSceneSystem());
    simulation.push_back(SpaceSystem::createSpaceEntitiesSystem());

    //Per-component deserializers
    simulation.push_back(TransformT::createDeserializer());

    simulation.push_back(SpaceSystem::completeSpaceLoadSystem());

    //Save space
    simulation.push_back(SpaceSystem::beginSaveSpaceSystem());
    simulation.push_back(SpaceSystem::createSerializedEntitiesSystem());

    //Per-component serializers
    simulation.push_back(TransformT::createSerializer());

    simulation.push_back(SpaceSystem::serializeSpaceSystem());
    simulation.push_back(SpaceSystem::completeSpaceSaveSystem());

    //Towards the end so it can get any messages before they are cleared
    simulation.push_back(FileSystemSystem::fileReader());
    simulation.push_back(FileSystemSystem::fileWriter());

    //Editor
    graphics.push_back(EditorSystem::sceneBrowser());

    graphics.push_back(GraphicsSystemBase::screenSizeListener());

    //Clear messages at the end of the frame
    cleanup.push_back(RemoveEntitiesSystem<View<Read<MessageComponent>>>::create());

    context.registerInitializer(std::move(initializers));
    context.registerUpdatePhase(AppPhase::Input, std::move(input), 60);
    context.registerUpdatePhase(AppPhase::Simulation, std::move(simulation), 20);
    context.registerUpdatePhase(AppPhase::Physics, std::move(physics), 60);
    context.registerUpdatePhase(AppPhase::Graphics, std::move(graphics), 60);
    context.registerUpdatePhase(AppPhase::Cleanup, std::move(cleanup), 0);

    context.buildExecutionGraph();
  }

  void registerSystems(const SystemArgs& args, ISystemRegistry& registry) override {
    registry;args;
    //auto loaders = Registry::createAssetLoaderRegistry();
    //registerAssetLoaders(*loaders);
    //registry.registerSystem(std::make_unique<ImGuiSystem>(args));
    //registry.registerSystem(std::make_unique<AssetRepo>(args, std::move(loaders)));
    //registry.registerSystem(std::make_unique<GraphicsSystem>(args));
    //registry.registerSystem(std::make_unique<LuaGameSystem>(args));
    //registry.registerSystem(std::make_unique<PhysicsSystem>(args));
    //registry.registerSystem(std::make_unique<Editor>(args));
  }

  void registerAssetLoaders(IAssetLoaderRegistry& registry) {
    registry;
    //registry.registerLoader<BufferAsset, BufferAssetLoader>("buff");
    //registry.registerLoader<TextAsset, TextAssetLoader>("txt");
    //registry.registerLoader<LuaScript, LuaScriptLoader>("lc");
    //registry.registerLoader<Model, ModelOBJLoader>("obj");
    //registry.registerLoader<Shader, ShaderLoader>("vs");
    //registry.registerLoader<Texture, TextureBMPLoader>("bmp");
  }

  void registerComponents(IComponentRegistry& registry) override {
    registry;
    //registry.registerComponent<CameraComponent>();
    //registry.registerComponent<LuaComponent>();
    //registry.registerComponent<NameComponent>();
    //registry.registerComponent<Physics>();
    //registry.registerComponent<Renderable>();
    //registry.registerComponent<SpaceComponent>();
    //registry.registerComponent<Transform>();
  }
};

namespace Registration {
  std::unique_ptr<AppRegistration> createDefaultApp() {
    return std::make_unique<DefaultAppRegistration>();
  }

  std::unique_ptr<AppRegistration> compose(std::shared_ptr<AppRegistration> a, std::shared_ptr<AppRegistration> b) {
    struct Composer : public AppRegistration {
      void registerAppContext(Engine::AppContext& context) override {
        Engine::AppContext::PhaseContainer input, simulation, graphics, physics, cleanup;
        Engine::SystemList empty, initializers;
        auto foreachPhase = [&](auto func) {
          func(Engine::AppPhase::Input, input);
          func(Engine::AppPhase::Simulation, simulation);
          func(Engine::AppPhase::Physics, physics);
          func(Engine::AppPhase::Graphics, graphics);
          func(Engine::AppPhase::Cleanup, cleanup);
        };
        auto resetRegisteredPhases = [&foreachPhase, &empty, &context] {
          foreachPhase([&empty, &context](Engine::AppPhase phase, const Engine::AppContext::PhaseContainer&) {
            context.registerUpdatePhase(phase, empty, 0);
          });
          context.registerInitializer(empty);
        };

        for(auto&& reg : mRegistration) {
          //Clear out any previously registered systems so we can determine what is added by AppRegistration
          resetRegisteredPhases();

          reg->registerAppContext(context);

          //Gather all the newly registered phases
          foreachPhase([&empty, &context](Engine::AppPhase phase, Engine::AppContext::PhaseContainer& container) {
            auto registeredPhase = context.getUpdatePhase(phase);
            if(!registeredPhase.mSystems.empty()) {
              //Arbitrarily take the latest
              container.mTargetFPS = registeredPhase.mTargetFPS;
              //This is assuming that the user of compose won't cause double-registration of systems
              container.mSystems.insert(container.mSystems.end(), registeredPhase.mSystems.begin(), registeredPhase.mSystems.end());
            }
          });
          Engine::SystemList newInitializers = context.getInitializers().mSystems;
          initializers.insert(initializers.end(), newInitializers.begin(), newInitializers.end());
        }

        //Register the final gathered systems
        resetRegisteredPhases();
        context.registerInitializer(std::move(initializers));
        foreachPhase([&context](Engine::AppPhase phase, const Engine::AppContext::PhaseContainer& container) {
          context.registerUpdatePhase(phase, container.mSystems, container.mTargetFPS);
        });

        context.buildExecutionGraph();
      }

      void registerSystems(const SystemArgs& args, ISystemRegistry& registry) override {
        for(auto&& reg : mRegistration) {
          reg->registerSystems(args, registry);
        }
      }

      void registerComponents(IComponentRegistry& registry) override {
        for(auto&& reg : mRegistration) {
          reg->registerComponents(registry);
        }
      }

      std::vector<std::shared_ptr<AppRegistration>> mRegistration;
    };

    auto result = std::make_unique<Composer>();
    result->mRegistration.push_back(std::move(a));
    result->mRegistration.push_back(std::move(b));
    return result;
  }
};
