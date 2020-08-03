#include "Precompile.h"
#include "component/Component.h"

#include "component/ComponentPublisher.h"
#include "event/BaseComponentEvents.h"
#include "event/EventBuffer.h"
#include "provider/MessageQueueProvider.h"
#include "lua/LuaCache.h"
#include "lua/LuaNode.h"
#include "lua/LuaGameContext.h"
#include "lua/LuaStackAssert.h"
#include "lua/LuaUtil.h"
#include "system/AssetRepo.h"
#include "system/LuaGameSystem.h"
#include "LuaGameObject.h"
#include "Util.h"

#include <lua.hpp>

const std::string Component::LUA_PROPS_KEY = "props";
const std::string Component::BASE_CLASS_NAME = "Component";
std::unique_ptr<Lua::Cache> Component::sLuaCache = std::make_unique<Lua::Cache>("_component_cache_", BASE_CLASS_NAME);

size_t ComponentType::operator()() const {
  std::hash<size_t> hasher;
  return Util::hashCombine(hasher(id), subId);
}

const bool ComponentType::operator==(const ComponentType& rhs) const {
  return id == rhs.id
    && subId == rhs.subId;
}

const bool ComponentType::operator!=(const ComponentType& rhs) const {
  return !(*this == rhs);
}

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
  , mSubType(0)
  , mCacheId(sLuaCache->nextHandle()) {
}

Component::~Component() {
}

const Lua::Node* Component::getLuaProps() const {
  return nullptr;
}

void Component::setSystem(System& system) {
  mSystem = &system;
}

System* Component::getSystem() const {
  return mSystem;
}

AssetRepo* Component::getAssetRepo() const {
  return AssetRepo::get();
}

int Component::push(lua_State* l) const {
  // Lua must take a mutable void pointer, but Component will only expose accessor as const when getting it back out, so const_cast is safe
  return sLuaCache->push(l, const_cast<Component*>(this), mCacheId, getTypeInfo().mTypeName.c_str());
}

ComponentPublisher Component::_checkSelf(lua_State* l, const std::string& type, int arg) {
  return ComponentPublisher(*static_cast<Component*>(sLuaCache->checkParam(l, arg, type.c_str())));
}

void Component::invalidate(lua_State* l) const {
  sLuaCache->invalidate(l, mCacheId, getTypeInfo().mTypeName.c_str());
}

void Component::addSync(EventBuffer& msg) const {
  msg.push(AddComponentEvent(getOwner(), getFullType().id, getFullType().subId));
  sync(msg, ~Lua::NodeDiff(0));
}

void Component::sync(EventBuffer& msg, Lua::NodeDiff diff) const {
  if(const Lua::Node* props = getLuaProps()) {
    std::vector<uint8_t> buffer(props->size());
    props->copyConstructToBuffer(this, buffer.data());
    msg.push(SetComponentPropsEvent(getOwner(), getFullType(), props, diff, std::move(buffer)));
  }
}

void Component::openLib(lua_State*) const {
}

const ComponentTypeInfo& Component::getTypeInfo() const {
  static ComponentTypeInfo result("");
  return result;
}

void Component::setPropsFromStack(lua_State* l, MessageQueueProvider& msg) const {
  if(const Lua::Node* props = getLuaProps()) {
    //Read the data into a copy and send an event with the change to apply it next frame
    //TODO: stack allocations
    std::unique_ptr<Component> copy = clone();
    lua_pushvalue(l, -1);
    props->readFromLua(l, copy.get(), Lua::Node::SourceType::FromStack);
    lua_pop(l, 1);
    std::vector<uint8_t> buffer(props->size());
    props->copyConstructToBuffer(copy.get(), &buffer[0]);
    msg.getMessageQueue().get().push(SetComponentPropsEvent(copy->getOwner(), copy->getFullType(), props, ~Lua::NodeDiff(0), std::move(buffer)));
  }
}

void Component::setPropFromStack(lua_State* l, const char* name, MessageQueueProvider& msg) const {
  if(const Lua::Node* props = getLuaProps()) {
    //TODO: support a way to get children several levels deep?
    if(const Lua::Node* foundProp = props->getChild(name)) {
      //Make a buffer big enough to hold the whole component
      //TODO: stack allocate
      std::vector<uint8_t> buff(props->size());
      //Translate to part of buffer where property goes
      void* propValue = foundProp->_translateBufferToNode(&buff[0]);

      //Write property value in buffer
      lua_pushvalue(l, 3);
      foundProp->readFromLuaToBuffer(l, propValue, Lua::Node::SourceType::FromStack);
      lua_pop(l, 1);
 
      //Send with diff indicating the appropriate part of the buffer
      msg.getMessageQueue().get().push(SetComponentPropsEvent(getOwner(), getFullType(), props, foundProp->_getDiffId(), std::move(buff)));
    }
  }
}

void Component::baseOpenLib(lua_State* l) {
  sLuaCache->createCache(l);
}

int Component::_getName(lua_State* l, const std::string& type) {
  ComponentPublisher self = _checkSelf(l, type);
  const std::string& name = self->getTypeInfo().mPropName;
  lua_pushlstring(l, name.c_str(), name.size());
  return 1;
}

int Component::_getType(lua_State* l, const std::string& type) {
  ComponentPublisher self = _checkSelf(l, type);
  const std::string& name = self->getTypeInfo().mTypeName;
  lua_pushlstring(l, name.c_str(), name.size());
  return 1;
}

int Component::_getOwner(lua_State* l, const std::string& type) {
  ComponentPublisher self = _checkSelf(l, type);
  if(LuaGameSystem* game = LuaGameSystem::get(l)) {
    if(LuaGameObject* obj = game->_getObj(self->getOwner())) {
      return LuaGameObject::push(l, *obj);
    }
  }
  return 0;
}

int Component::_getProps(lua_State* l, const std::string& type) {
  Lua::StackAssert sa(l, 1);
  ComponentPublisher self = _checkSelf(l, type);
  if(const Lua::Node* props = self->getLuaProps()) {
    props->writeToLua(l, self.get());
  }
  else {
    lua_newtable(l);
  }
  return 1;
}

int Component::_setProps(lua_State* l, const std::string& type) {
  Lua::StackAssert sa(l);
  ComponentPublisher self = _checkSelf(l, type);
  ILuaGameContext& game = Lua::checkGameContext(l);
  luaL_checktype(l, 2, LUA_TTABLE);

  lua_pushvalue(l, 2);
  self->setPropsFromStack(l, game.getMessageProvider());
  lua_pop(l, 1);
  return 0;
}

int Component::_getProp(lua_State* l, const std::string& type) {
  Lua::StackAssert sa(l, 1);
  ComponentPublisher self = _checkSelf(l, type);
  const char* propName = luaL_checkstring(l, 2);

  if(const Lua::Node* props = self->getLuaProps()) {
    //TODO: support a way to get children several levels deep?
    if(const Lua::Node* foundProp = props->getChild(propName)) {
      foundProp->writeToLua(l, self.get());
      return 1;
    }
  }
  //Property not found, return null
  lua_pushnil(l);
  return 1;
}

int Component::_setProp(lua_State* l, const std::string& type) {
  Lua::StackAssert sa(l);
  ComponentPublisher self = _checkSelf(l, type);
  const char* propName = luaL_checkstring(l, 2);
  ILuaGameContext& game = Lua::checkGameContext(l);

  self->setPropFromStack(l, propName, game.getMessageProvider());
  return 0;
}

int Component::_indexOverload(lua_State* l, const std::string& type) {
  ComponentPublisher self = _checkSelf(l, type);
  const char* propName = luaL_checkstring(l, 2);
  //Determine if they're accessinga  property or calling a function
  //Property exists, access property
  if(self->_getPropByName(propName))
    return _getProp(l, type);
  //Property doesn't exist, fall back to default, which is most likely a function
  return Lua::Util::defaultIndex(l);
}

int Component::_newIndexOverload(lua_State* l, const std::string& type) {
  return _setProp(l, type);
}

const Lua::Node* Component::_getPropByName(const char* propName) const {
  if(const Lua::Node* props = getLuaProps())
    return props->getChild(propName);
  return nullptr;
}

const Lua::Cache& Component::getLuaCache() {
  return *sLuaCache;
}
