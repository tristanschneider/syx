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

Component::Component(Handle type, Handle owner)
  : mOwner(owner)
  , mType(type)
  , mSubType(0)
  , mCacheId(sLuaCache->nextHandle()) {
}

Component::~Component() {
}

void Component::set(const Component& component) {
  assert(getFullType() == component.getFullType() && "Caller should make ssure types match before calling set");
  if (const Lua::Node* props = component.getLuaProps()) {
    std::vector<uint8_t> buffer(props->size());
    props->copyToBuffer(&component, buffer.data());
    props->copyFromBuffer(this, buffer.data());
  }
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

int Component::push(lua_State* l, IComponent& component) {
  // Lua must take a mutable void pointer, but Component will only expose accessor as const when getting it back out, so const_cast is safe
  return sLuaCache->push(l, &component, component.get().mCacheId, component.get().getTypeInfo().mTypeName.c_str());
}

IComponent& Component::_checkSelf(lua_State* l, const std::string& type, int arg) {
  return *static_cast<IComponent*>(sLuaCache->checkParam(l, arg, type.c_str()));
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

void Component::setPropsFromStack(lua_State* l, IComponent& component) {
  const Component& self = component.get();
  if(const Lua::Node* props = self.getLuaProps()) {
    //Read the data into a copy and send an event with the change to apply it next frame
    std::unique_ptr<Component> copy = self.clone();

    lua_pushvalue(l, -1);
    props->readFromLua(l, copy.get(), Lua::Node::SourceType::FromStack);
    lua_pop(l, 1);

    component.set(*copy);
  }
}

void Component::setPropFromStack(lua_State* l, IComponent& component, const char* name) {
  const Component& self = component.get();
  if(const Lua::Node* props = self.getLuaProps()) {
    //TODO: support a way to get children several levels deep?
    if(const Lua::Node* foundProp = props->getChild(name)) {
      //Make a buffer big enough to hold the whole component
      std::vector<uint8_t> buff(props->size());
      //Translate to part of buffer where property goes
      std::unique_ptr<Component> mutableCopy = self.clone();
      void* propValue = foundProp->_translateBaseToNode(mutableCopy.get());

      //Write property value in copy
      lua_pushvalue(l, 3);
      foundProp->readFromLua(l, propValue, Lua::Node::SourceType::FromStack);
      lua_pop(l, 1);

      component.set(*mutableCopy);
    }
  }
}

void Component::baseOpenLib(lua_State* l) {
  sLuaCache->createCache(l);
}

int Component::_getName(lua_State* l, const std::string& type) {
  IComponent& self = _checkSelf(l, type);
  const std::string& name = self.get().getTypeInfo().mPropName;
  lua_pushlstring(l, name.c_str(), name.size());
  return 1;
}

int Component::_getType(lua_State* l, const std::string& type) {
  IComponent& self = _checkSelf(l, type);
  const std::string& name = self.get().getTypeInfo().mTypeName;
  lua_pushlstring(l, name.c_str(), name.size());
  return 1;
}

int Component::_getOwner(lua_State* l, const std::string& type) {
  IComponent& self = _checkSelf(l, type);
  ILuaGameContext& game = Lua::checkGameContext(l);
  if(IGameObject* obj = game.getGameObject(self.get().getOwner())) {
    return LuaGameObject::push(l, *obj);
  }
  return 0;
}

int Component::_getProps(lua_State* l, const std::string& type) {
  Lua::StackAssert sa(l, 1);
  IComponent& self = _checkSelf(l, type);
  if(const Lua::Node* props = self.get().getLuaProps()) {
    props->writeToLua(l, &self.get());
  }
  else {
    lua_newtable(l);
  }
  return 1;
}

int Component::_setProps(lua_State* l, const std::string& type) {
  Lua::StackAssert sa(l);
  IComponent& self = _checkSelf(l, type);
  luaL_checktype(l, 2, LUA_TTABLE);

  lua_pushvalue(l, 2);
  setPropsFromStack(l, self);
  lua_pop(l, 1);
  return 0;
}

int Component::_getProp(lua_State* l, const std::string& type) {
  Lua::StackAssert sa(l, 1);
  IComponent& self = _checkSelf(l, type);
  const char* propName = luaL_checkstring(l, 2);

  if(const Lua::Node* props = self.get().getLuaProps()) {
    //TODO: support a way to get children several levels deep?
    if(const Lua::Node* foundProp = props->getChild(propName)) {
      foundProp->writeToLua(l, &self.get());
      return 1;
    }
  }
  //Property not found, return null
  lua_pushnil(l);
  return 1;
}

int Component::_setProp(lua_State* l, const std::string& type) {
  Lua::StackAssert sa(l);
  IComponent& self = _checkSelf(l, type);
  const char* propName = luaL_checkstring(l, 2);

  setPropFromStack(l, self, propName);
  return 0;
}

int Component::_indexOverload(lua_State* l, const std::string& type) {
  IComponent& self = _checkSelf(l, type);
  const char* propName = luaL_checkstring(l, 2);
  //Determine if they're accessinga  property or calling a function
  //Property exists, access property
  if(self.get()._getPropByName(propName))
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
