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
#include "system/KeyboardInput.h"
#include "system/LuaGameSystem.h"
#include "system/PhysicsSystem.h"
#include "loader/AssetLoader.h"
#include "loader/LuaScriptLoader.h"
#include "loader/ModelLoader.h"
#include "loader/ShaderLoader.h"
#include "loader/TextureLoader.h"

class DefaultAppRegistration : public AppRegistration {
public:
  void registerSystems(const SystemArgs& args, ISystemRegistry& registry) override {
    auto loaders = Registry::createAssetLoaderRegistry();
    registerAssetLoaders(*loaders);
    registry.registerSystem(std::make_unique<ImGuiSystem>(args));
    registry.registerSystem(std::make_unique<AssetRepo>(args, std::move(loaders)));
    registry.registerSystem(std::make_unique<GraphicsSystem>(args));
    registry.registerSystem(std::make_unique<KeyboardInput>(args));
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
