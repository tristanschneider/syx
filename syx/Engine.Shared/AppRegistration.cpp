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

void AppRegistration::registerSystems() {
  AssetRepo::sSystemReg.registerSystem();
  GraphicsSystem::sSystemReg.registerSystem();
  KeyboardInput::sSystemReg.registerSystem();
  LuaGameSystem::sSystemReg.registerSystem();
  PhysicsSystem::sSystemReg.registerSystem();
  Editor::sSystemReg.registerSystem();
}

void AppRegistration::registerAssetLoaders() {
  AssetRepo::registerLoader<BufferAsset, BufferAssetLoader>("buff");

  AssetRepo::registerLoader<TextAsset, TextAssetLoader>("txt");
  AssetRepo::registerLoader<LuaScript, LuaScriptLoader>("lc");
  AssetRepo::registerLoader<Model, ModelOBJLoader>("obj");
  AssetRepo::registerLoader<Shader, ShaderLoader>("vs");
  AssetRepo::registerLoader<Texture, TextureBMPLoader>("bmp");
}
