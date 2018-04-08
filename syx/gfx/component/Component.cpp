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

#include <lua.hpp>

const std::string Component::LUA_PROPS_KEY = "props";
const std::string Component::BASE_CLASS_NAME = "Component";
std::unique_ptr<Lua::Cache> Component::sLuaCache = std::make_unique<Lua::Cache>("_component_cache_", BASE_CLASS_NAME);

Component::Registry::Registrar::Registrar(size_t type, Constructor ctor) {
  Component::Registry::registerComponent(type, ctor);
}

void Component::Registry::registerComponent(size_t type, Constructor ctor) {
  _get().mCtors[type] = ctor;
}

std::unique_ptr<Component> Component::Registry::construct(size_t type, Handle owner) {
  return _get().mCtors[type](owner);
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
  return sLuaCache->push(l, this, mCacheId, getTypeName().c_str());
}

void Component::invalidate(lua_State* l) const {
  sLuaCache->invalidate(l, mCacheId, getTypeName().c_str());
}

void Component::openLib(lua_State* l) const {
}

const std::string& Component::getName() const {
  static std::string empty;
  return empty;
}

const std::string& Component::getTypeName() const {
  static std::string empty;
  return empty;
}

size_t Component::getNameConstHash() const {
  return 0;
}

void Component::baseOpenLib(lua_State* l) {
  sLuaCache->createCache(l);
}

int Component::getName(lua_State* l, const std::string& type) {
  Component* self = static_cast<Component*>(sLuaCache->checkParam(l, 1, type.c_str()));
  const std::string& name = self->getName();
  lua_pushlstring(l, name.c_str(), name.size());
  return 1;
}

int Component::getType(lua_State* l, const std::string& type) {
  Component* self = static_cast<Component*>(sLuaCache->checkParam(l, 1, type.c_str()));
  const std::string& name = self->getTypeName();
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
    props->write(l, reinterpret_cast<uint8_t*>(self));
  }
  else {
    lua_newtable(l);
  }
  return 1;
}

const Lua::Cache& Component::getLuaCache() const {
  return *sLuaCache;
}
