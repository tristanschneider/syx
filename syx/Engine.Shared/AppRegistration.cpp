#include "Precompile.h"
#include "AppRegistration.h"

#include "asset/LuaScript.h"
#include "asset/Model.h"
#include "asset/PhysicsModel.h"
#include "asset/Shader.h"
#include "asset/Texture.h"

#include "system/AssetRepo.h"
#include "system/GraphicsSystem.h"
#include "system/KeyboardInput.h"
#include "system/LuaGameSystem.h"
#include "system/PhysicsSystem.h"
#include "editor/Editor.h"

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
    registry.registerSystem(std::make_unique<AssetRepo>(args, std::move(loaders)));
    registry.registerSystem(std::make_unique<GraphicsSystem>(args));
    registry.registerSystem(std::make_unique<KeyboardInput>(args));
    registry.registerSystem(std::make_unique<LuaGameSystem>(args));
    registry.registerSystem(std::make_unique<PhysicsSystem>(args));
    registry.registerSystem(std::make_unique<Editor>(args));
  }

  void registerAssetLoaders(IAssetLoaderRegistry& registry) override {
    registry.registerLoader<BufferAsset, BufferAssetLoader>("buff");
    registry.registerLoader<TextAsset, TextAssetLoader>("txt");
    registry.registerLoader<LuaScript, LuaScriptLoader>("lc");
    registry.registerLoader<Model, ModelOBJLoader>("obj");
    registry.registerLoader<Shader, ShaderLoader>("vs");
    registry.registerLoader<Texture, TextureBMPLoader>("bmp");
  }
};

namespace Registration {
  std::unique_ptr<AppRegistration> createDefaultApp() {
    return std::make_unique<DefaultAppRegistration>();
  }
};
