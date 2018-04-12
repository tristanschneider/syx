#include "Precompile.h"
#include "component/Component.h"

#include "event/BaseComponentEvents.h"
#include "event/EventBuffer.h"
#include "provider/MessageQueueProvider.h"
#include "lua/LuaCache.h"
#include "lua/LuaNode.h"
#include "lua/LuaStackAssert.h"
#include "system/LuaGameSystem.h"
#include "LuaGameObject.h"
#include "Util.h"

#include <lua.hpp>

const std::string Component::LUA_PROPS_KEY = "props";
const std::string Component::BASE_CLASS_NAME = "Component";
std::unique_ptr<Lua::Cache> Component::sLuaCache = std::make_unique<Lua::Cache>("_component_cache_", BASE_CLASS_NAME);

ComponentTypeInfo::ComponentTypeInfo(const std::string& typeName)
  : mTypeName(typeName)
  , mPropName(typeName) {
  //Property name is just typename that starts with lowercase
  if(mPropName.size())
    mPropName.front() -= 'A' - 'a';
  mPropNameConstHash = Util::constHash(mPropName.c_str());
}

Component::Registry::Registrar::Registrar(size_t type, Constructor ctor) {
  Component::Registry::registerComponent(type, ctor);
}

void Component::Registry::registerComponent(size_t type, Constructor ctor) {
  _get().mCtors[type] = ctor;
}

std::unique_ptr<Component> Component::Registry::construct(size_t type, Handle owner) {
  return _get().mCtors[type](owner);
}

const TypeMap<Component::Registry::Constructor, Component>& Component::Registry::getConstructors() {
  return _get().mCtors;
}

Component::Registry::Registry() {
}

Component::Registry& Component::Registry::_get() {
  static Registry singleton;
  return singleton;
}

Component::Component(Handle type, Handle owner)
  : mOwner(owner)
  , mType(type)
  , mCacheId(sLuaCache->nextHandle()) {
}

Component::~Component() {
}

const Lua::Node* Component::getLuaProps() const {
  return nullptr;
}

int Component::push(lua_State* l) {
  return sLuaCache->push(l, this, mCacheId, getTypeInfo().mTypeName.c_str());
}

void Component::invalidate(lua_State* l) const {
  sLuaCache->invalidate(l, mCacheId, getTypeInfo().mTypeName.c_str());
}

void Component::openLib(lua_State* l) const {
}

const ComponentTypeInfo& Component::getTypeInfo() const {
  static ComponentTypeInfo result("");
  return result;
}

void Component::baseOpenLib(lua_State* l) {
  sLuaCache->createCache(l);
}

int Component::getName(lua_State* l, const std::string& type) {
  Component* self = static_cast<Component*>(sLuaCache->checkParam(l, 1, type.c_str()));
  const std::string& name = self->getTypeInfo().mPropName;
  lua_pushlstring(l, name.c_str(), name.size());
  return 1;
}

int Component::getType(lua_State* l, const std::string& type) {
  Component* self = static_cast<Component*>(sLuaCache->checkParam(l, 1, type.c_str()));
  const std::string& name = self->getTypeInfo().mTypeName;
  lua_pushlstring(l, name.c_str(), name.size());
  return 1;
}

int Component::getOwner(lua_State* l, const std::string& type) {
  Component* self = static_cast<Component*>(sLuaCache->checkParam(l, 1, type.c_str()));
  if(LuaGameSystem* game = LuaGameSystem::get(l)) {
    if(LuaGameObject* obj = game->_getObj(self->getOwner())) {
      return LuaGameObject::push(l, *obj);
    }
  }
  return 0;
}

int Component::getProps(lua_State* l, const std::string& type) {
  Lua::StackAssert sa(l, 1);
  Component* self = static_cast<Component*>(sLuaCache->checkParam(l, 1, type.c_str()));
  if(const Lua::Node* props = self->getLuaProps()) {
    props->writeToLua(l, self);
  }
  else {
    lua_newtable(l);
  }
  return 1;
}

const Lua::Cache& Component::getLuaCache() const {
  return *sLuaCache;
}
