#include "Precompile.h"
#include "AppRegistration.h"

#include "asset/LuaScript.h"
#include "asset/Model.h"
#include "asset/PhysicsModel.h"
#include "asset/Shader.h"
#include "asset/Texture.h"
#include "editor/Editor.h"
#include "component/CameraComponent.h"
#include "component/LuaComponent.h"
#include "component/LuaComponentRegistry.h"
#include "component/NameComponent.h"
#include "component/Physics.h"
#include "component/Renderable.h"
#include "component/SpaceComponent.h"
#include "component/Transform.h"
#include "system/AssetRepo.h"
#include "system/GraphicsSystem.h"
#include "system/ImGuiSystem.h"
#include "system/LuaGameSystem.h"
#include "system/PhysicsSystem.h"
#include "loader/AssetLoader.h"
#include "loader/LuaScriptLoader.h"
#include "loader/ModelLoader.h"
#include "loader/ShaderLoader.h"
#include "loader/TextureLoader.h"

class DefaultAppRegistration : public AppRegistration {
public:
  void registerAppContext(Engine::AppContext& context) override {
    Engine::SystemList input, simulation, graphics, physics;

    //TODO: register systems here

    context.registerUpdatePhase(Engine::AppPhase::Input, std::move(input), 60);
    context.registerUpdatePhase(Engine::AppPhase::Simulation, std::move(simulation), 20);
    context.registerUpdatePhase(Engine::AppPhase::Physics, std::move(physics), 60);
    context.registerUpdatePhase(Engine::AppPhase::Graphics, std::move(graphics), 60);

    context.buildExecutionGraph();
  }

  void registerSystems(const SystemArgs& args, ISystemRegistry& registry) override {
    auto loaders = Registry::createAssetLoaderRegistry();
    registerAssetLoaders(*loaders);
    registry.registerSystem(std::make_unique<ImGuiSystem>(args));
    registry.registerSystem(std::make_unique<AssetRepo>(args, std::move(loaders)));
    registry.registerSystem(std::make_unique<GraphicsSystem>(args));
    registry.registerSystem(std::make_unique<LuaGameSystem>(args));
    registry.registerSystem(std::make_unique<PhysicsSystem>(args));
    registry.registerSystem(std::make_unique<Editor>(args));
  }

  void registerAssetLoaders(IAssetLoaderRegistry& registry) {
    registry.registerLoader<BufferAsset, BufferAssetLoader>("buff");
    registry.registerLoader<TextAsset, TextAssetLoader>("txt");
    registry.registerLoader<LuaScript, LuaScriptLoader>("lc");
    registry.registerLoader<Model, ModelOBJLoader>("obj");
    registry.registerLoader<Shader, ShaderLoader>("vs");
    registry.registerLoader<Texture, TextureBMPLoader>("bmp");
  }

  void registerComponents(IComponentRegistry& registry) override {
    registry.registerComponent<CameraComponent>();
    registry.registerComponent<LuaComponent>();
    registry.registerComponent<NameComponent>();
    registry.registerComponent<Physics>();
    registry.registerComponent<Renderable>();
    registry.registerComponent<SpaceComponent>();
    registry.registerComponent<Transform>();
  }
};

namespace Registration {
  std::unique_ptr<AppRegistration> createDefaultApp() {
    return std::make_unique<DefaultAppRegistration>();
  }

  std::unique_ptr<AppRegistration> compose(std::shared_ptr<AppRegistration> a, std::shared_ptr<AppRegistration> b) {
    struct Composer : public AppRegistration {
      void registerAppContext(Engine::AppContext& context) override {
        Engine::AppContext::PhaseContainer input, simulation, graphics, physics;
        Engine::SystemList empty;
        auto foreachPhase = [&](auto func) {
          func(Engine::AppPhase::Input, input);
          func(Engine::AppPhase::Simulation, simulation);
          func(Engine::AppPhase::Physics, physics);
          func(Engine::AppPhase::Graphics, graphics);
        };
        auto resetRegisteredPhases = [&foreachPhase, &empty, &context] {
          foreachPhase([&empty, &context](Engine::AppPhase phase, const Engine::AppContext::PhaseContainer&) {
            context.registerUpdatePhase(phase, empty, 0);
          });
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
        }

        //Register the final gathered systems
        resetRegisteredPhases();
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
