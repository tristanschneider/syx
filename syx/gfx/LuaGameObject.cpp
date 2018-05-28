#include "Precompile.h"
#include "LuaGameObject.h"

#include "component/LuaComponent.h"
#include "lua/LuaCache.h"
#include "lua/LuaComponentNode.h"
#include "lua/LuaCompositeNodes.h"
#include "lua/LuaStackAssert.h"
#include "lua/LuaUtil.h"
#include <lua.hpp>
#include "system/LuaGameSystem.h"

const std::string LuaGameObject::CLASS_NAME = "Gameobject";
std::unique_ptr<Lua::Cache> LuaGameObject::sCache = std::make_unique<Lua::Cache>("_goc_", CLASS_NAME);

LuaGameObject::LuaGameObject(Handle h)
  : mHandle(h)
  , mTransform(h) {
}

LuaGameObject::~LuaGameObject() {
}

Handle LuaGameObject::getHandle() const {
  return mHandle;
}

void LuaGameObject::addComponent(std::unique_ptr<Component> component) {
  mHashToComponent[component->getTypeInfo().mPropNameConstHash] = component.get();
  mComponents[component->getType()] = std::move(component);
}

void LuaGameObject::removeComponent(size_t type) {
  if(std::unique_ptr<Component>* component = mComponents.get(type)) {
    if(*component) {
      mHashToComponent.erase((*component)->getTypeInfo().mPropNameConstHash);
      *component = nullptr;
    }
  }
}

Component* LuaGameObject::getComponent(size_t type) {
  return mComponents[type].get();
}

Transform& LuaGameObject::getTransform() {
  return mTransform;
}

const Transform& LuaGameObject::getTransform() const {
  return mTransform;
}

LuaComponent* LuaGameObject::addLuaComponent(size_t script) {
  auto it = mLuaComponents.emplace(script, mHandle);
  LuaComponent& result = it.first->second;
  result.setScript(script);
  mHashToComponent[result.getTypeInfo().mPropNameConstHash] = &result;
  return &result;
}

LuaComponent* LuaGameObject::getLuaComponent(size_t script) {
  auto it = mLuaComponents.find(script);
  return it != mLuaComponents.end() ? &it->second : nullptr;
}

void LuaGameObject::removeLuaComponent(size_t script) {
  auto it = mLuaComponents.find(script);
  if(it != mLuaComponents.end()) {
    mHashToComponent.erase(it->second.getTypeInfo().mPropNameConstHash);
    mLuaComponents.erase(it);
  }
}

std::unordered_map<size_t, LuaComponent>& LuaGameObject::getLuaComponents() {
  return mLuaComponents;
}

const std::unordered_map<size_t, LuaComponent>& LuaGameObject::getLuaComponents() const {
  return mLuaComponents;
}

const TypeMap<std::unique_ptr<Component>, Component>& LuaGameObject::getComponents() const {
  return mComponents;
}

void LuaGameObject::openLib(lua_State* l) {
  luaL_Reg statics[] = {
    { nullptr, nullptr }
  };
  luaL_Reg members[] = {
    { "__index", &indexOverload },
    { "__tostring", &toString },
    { "addComponent", &addComponent },
    { "removeComponent", &removeComponent },
    { "isValid", &isValid },
    { nullptr, nullptr }
  };
  Lua::Util::registerClass(l, statics, members, CLASS_NAME.c_str());

  sCache->createCache(l);
}

int LuaGameObject::toString(lua_State* l) {
  LuaGameObject& self = getObj(l, 1);
  std::string result = CLASS_NAME + "( " + std::to_string(self.mHandle) + ")";
  lua_pushlstring(l, result.c_str(), result.size());
  return 1;
}

int LuaGameObject::indexOverload(lua_State* l) {
  LuaGameObject& obj = getObj(l, 1);
  const char* key = luaL_checkstring(l, 2);
  //If key is a known function, fall back to default behavior and call that
  size_t nameHash = Util::constHash(key);
  switch(nameHash) {
    case Util::constHash("addComponent"):
    case Util::constHash("removeComponent"):
    case Util::constHash("isValid"):
      return Lua::Util::defaultIndex(l);
    default:
      break;
  }

  //If key isn't known, assume it's a component name and try to find it
  auto it = obj.mHashToComponent.find(nameHash);
  if(it != obj.mHashToComponent.end()) {
    it->second->push(l);
  }
  else {
    //No component by this name, return null
    lua_pushnil(l);
  }
  return 1;
}

int LuaGameObject::addComponent(lua_State* l) {
  LuaGameObject& obj = getObj(l, 1);
  const char* componentName = luaL_checkstring(l, 2);
  LuaGameSystem& game = LuaGameSystem::check(l);
  if(Component* result = game.addComponent(componentName, obj.getHandle()))
    result->push(l);
  else
    lua_pushnil(l);
  return 1;
}

int LuaGameObject::removeComponent(lua_State* l) {
  LuaGameObject& obj = getObj(l, 1);
  const char* componentName = luaL_checkstring(l, 2);
  LuaGameSystem& game = LuaGameSystem::check(l);
  game.removeComponent(componentName, obj.getHandle());
  return 0;
}

int LuaGameObject::isValid(lua_State* l) {
  lua_pushboolean(l, sCache->getParam(l, 1) != nullptr);
  return 1;
}

int LuaGameObject::newDefault(lua_State* l) {
  Lua::StackAssert sa(l, 1);
  LuaGameSystem& game = LuaGameSystem::check(l);
  LuaGameObject::push(l, game.addGameObject());
  return 1;
}

int LuaGameObject::push(lua_State* l, LuaGameObject& obj) {
  return sCache->push(l, &obj, obj.mHandle);
}

int LuaGameObject::invalidate(lua_State* l, LuaGameObject& obj) {
  sCache->invalidate(l, obj.mHandle);
  for(auto& comp : obj.getComponents()) {
    comp->invalidate(l);
  }
  for(auto& comp : obj.getLuaComponents()) {
    comp.second.invalidate(l);
  }
  return 0;
}

LuaGameObject& LuaGameObject::getObj(lua_State* l, int index) {
  return *static_cast<LuaGameObject*>(sCache->checkParam(l, index));
}

const Lua::Node& LuaGameObjectDescription::getMetadata() const {
  static std::unique_ptr<Lua::Node> root = _buildMetadata();
  return *root;
}

std::unique_ptr<Lua::Node> LuaGameObjectDescription::_buildMetadata() const {
  using namespace Lua;
  auto root = makeRootNode(NodeOps(""));
  makeNode<SizetNode>(NodeOps(*root.get(), "handle", ::Util::offsetOf(*this, mHandle)));
  makeNode<VectorNode<ComponentNode>>(NodeOps(*root.get(), "components", ::Util::offsetOf(*this, mComponents)));
  return root;
}
