#include "Precompile.h"
#include "LuaGameObject.h"

#include "component/LuaComponent.h"
#include "lua/LuaStackAssert.h"
#include "lua/LuaUtil.h"
#include "lua/LuaCache.h"
#include <lua.hpp>

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
  mHashToComponent[component->getNameConstHash()] = component.get();
  mComponents[component->getType()] = std::move(component);
}

void LuaGameObject::removeComponent(size_t type) {
  if(std::unique_ptr<Component>* component = mComponents.get(type)) {
    mHashToComponent.erase((*component)->getNameConstHash());
    *component = nullptr;
  }
}

Component* LuaGameObject::getComponent(size_t type) {
  return mComponents[type].get();
}

Transform& LuaGameObject::getTransform() {
  return mTransform;
}

LuaComponent* LuaGameObject::addLuaComponent(size_t script) {
  auto it = mLuaComponents.emplace(script, mHandle);
  LuaComponent& result = it.first->second;
  result.setScript(script);
  mHashToComponent[result.getNameConstHash()] = &result;
  return &result;
}

LuaComponent* LuaGameObject::getLuaComponent(size_t script) {
  auto it = mLuaComponents.find(script);
  return it != mLuaComponents.end() ? &it->second : nullptr;
}

void LuaGameObject::removeLuaComponent(size_t script) {
  auto it = mLuaComponents.find(script);
  if(it != mLuaComponents.end()) {
    mHashToComponent.erase(it->second.getNameConstHash());
    mLuaComponents.erase(it);
  }
}

std::unordered_map<size_t, LuaComponent>& LuaGameObject::getLuaComponents() {
  return mLuaComponents;
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
  return 0;
}

int LuaGameObject::removeComponent(lua_State* l) {
  return 0;
}

int LuaGameObject::isValid(lua_State* l) {
  lua_pushboolean(l, sCache->getParam(l, 1) != nullptr);
  return 1;
}

int LuaGameObject::push(lua_State* l, LuaGameObject& obj) {
  return sCache->push(l, &obj, obj.mHandle);
}

int LuaGameObject::invalidate(lua_State* l, LuaGameObject& obj) {
  return sCache->invalidate(l, obj.mHandle);
}

LuaGameObject& LuaGameObject::getObj(lua_State* l, int index) {
  return *static_cast<LuaGameObject*>(sCache->checkParam(l, index));
}
