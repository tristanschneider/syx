#include "Precompile.h"
#include "LuaGameObject.h"

#include "component/LuaComponent.h"
#include "component/SpaceComponent.h"
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
  , mTransform(h)
  , mSpace(h) {
  _addBuiltInComponents();
}

LuaGameObject::~LuaGameObject() {
}

void LuaGameObject::_addBuiltInComponents() {
  _addComponentLookup(mTransform);
  _addComponentLookup(mSpace);
}

Handle LuaGameObject::getHandle() const {
  return mHandle;
}

void LuaGameObject::addComponent(std::unique_ptr<Component> component) {
  if(component->getType() == Component::typeId<LuaComponent>()) {
    addLuaComponent(component->getSubType());
  }
  else {
    _addComponentLookup(*component);
    mComponents[component->getType()] = std::move(component);
  }
}

void LuaGameObject::removeComponent(size_t type) {
  if(std::unique_ptr<Component>* component = mComponents.get(type)) {
    if(*component) {
      _removeComponentLookup(**component);
      *component = nullptr;
    }
  }
}

Component* LuaGameObject::getComponent(size_t type, size_t subType) {
  return const_cast<Component*>(const_cast<const LuaGameObject*>(this)->getComponent(type, subType));
}

const Component* LuaGameObject::getComponent(size_t type, size_t subType) const {
  if(type == Component::typeId<Transform>()) {
    return &mTransform;
  }
  if(type == Component::typeId<SpaceComponent>()) {
    return &mSpace;
  }
  if(type == Component::typeId<LuaComponent>()) {
    return getLuaComponent(subType);
  }
  auto ptr = mComponents.get(type);
  return ptr ? ptr->get() : nullptr;
}

Transform& LuaGameObject::getTransform() {
  return mTransform;
}

const Transform& LuaGameObject::getTransform() const {
  return mTransform;
}

Handle LuaGameObject::getSpace() const {
  return mSpace.get();
}

LuaComponent* LuaGameObject::addLuaComponent(size_t script) {
  auto it = mLuaComponents.emplace(script, mHandle);
  LuaComponent& result = it.first->second;
  result.setScript(script);
  _addComponentLookup(result);
  return &result;
}

LuaComponent* LuaGameObject::getLuaComponent(size_t script) {
  return const_cast<LuaComponent*>(const_cast<const LuaGameObject*>(this)->getLuaComponent(script));
}

const LuaComponent* LuaGameObject::getLuaComponent(size_t script) const {
  auto it = mLuaComponents.find(script);
  return it != mLuaComponents.end() ? &it->second : nullptr;
}

void LuaGameObject::removeLuaComponent(size_t script) {
  auto it = mLuaComponents.find(script);
  if(it != mLuaComponents.end()) {
    _removeComponentLookup(it->second);
    mLuaComponents.erase(it);
  }
}

void LuaGameObject::_addComponentLookup(Component& comp) {
  mHashToComponent[comp.getTypeInfo().mPropNameConstHash] = &comp;
}

void LuaGameObject::_removeComponentLookup(const Component& comp) {
  auto it = mHashToComponent.find(comp.getTypeInfo().mPropNameConstHash);
  if(it != mHashToComponent.end())
    mHashToComponent.erase(it);
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

void LuaGameObject::forEachComponent(std::function<void(const Component&)> callback) const {
  for(const auto& c : mComponents)
    callback(*c);
  for(const auto& c : mLuaComponents)
    callback(c.second);
  callback(mTransform);
  callback(mSpace);
}

void LuaGameObject::openLib(lua_State* l) {
  luaL_Reg statics[] = {
    { nullptr, nullptr }
  };
  luaL_Reg members[] = {
    { "__index", indexOverload },
    { "__newindex", newIndexOverload },
    { "__tostring", toString },
    { "addComponent", addComponent },
    { "removeComponent", removeComponent },
    { "isValid", isValid },
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

int LuaGameObject::newIndexOverload(lua_State* l) {
  LuaGameObject& self = getObj(l, 1);
  const char* key = luaL_checkstring(l, 2);
  int valueType = lua_type(l, 3);
  luaL_argcheck(l, valueType == LUA_TTABLE || valueType == LUA_TNIL, 3, "nil or table expected");
  LuaGameSystem& game = LuaGameSystem::check(l);

  //Trying to add component
  if(valueType == LUA_TTABLE) {
    Component* comp = game.addComponentFromPropName(key, self);
    luaL_argcheck(l, comp != nullptr, 2, "no such component exists");
    lua_pushvalue(l, 3);
    comp->setPropsFromStack(l, game);
    lua_pop(l, 1);
  }
  //Trying to remove component. Setting invalid component name to nil is fine, as it wouldn't do anything
  else {
    game.removeComponentFromPropName(key, self.getHandle());
  }
  return 0;
}

int LuaGameObject::addComponent(lua_State* l) {
  LuaGameObject& obj = getObj(l, 1);
  const char* componentName = luaL_checkstring(l, 2);
  LuaGameSystem& game = LuaGameSystem::check(l);
  if(Component* result = game.addComponent(componentName, obj))
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
  obj.forEachComponent([l](const Component& comp) {
    comp.invalidate(l);
  });
  return 0;
}

LuaGameObject& LuaGameObject::getObj(lua_State* l, int index) {
  return *static_cast<LuaGameObject*>(sCache->checkParam(l, index));
}

class LuaGameObjectDescriptionNode : public Lua::TypedNode<LuaGameObjectDescription> {
public:
  using TypedNode::TypedNode;
  void _readFromLua(lua_State* s, void* base) const override {
    LuaGameObjectDescription& desc = _cast(base);
    desc.getMetadata().readFromLua(s, base, Lua::Node::SourceType::FromStack);
  }
  void _writeToLua(lua_State* s, const void* base) const override {
    const LuaGameObjectDescription& desc = _cast(base);
    desc.getMetadata().writeToLua(s, base, Lua::Node::SourceType::FromStack);
  }
};

const Lua::Node& LuaGameObjectDescription::getMetadata() const {
  static std::unique_ptr<Lua::Node> root = _buildMetadata();
  return *root;
}

std::unique_ptr<Lua::Node> LuaGameObjectDescription::_buildMetadata() const {
  using namespace Lua;
  auto root = makeRootNode(NodeOps(""));
  makeNode<SizetNode>(NodeOps(*root, "handle", ::Util::offsetOf(*this, mHandle)));
  makeNode<VectorNode<ComponentNode>>(NodeOps(*root, "components", ::Util::offsetOf(*this, mComponents)));
  return root;
}

const char* LuaSceneDescription::ROOT_KEY = "scene";
const char* LuaSceneDescription::FILE_EXTENSION = "ls";

const Lua::Node& LuaSceneDescription::getMetadata() const {
  static std::unique_ptr<Lua::Node> root = _buildMetadata();
  return *root;
}

std::unique_ptr<Lua::Node> LuaSceneDescription::_buildMetadata() const {
  using namespace Lua;
  auto root = makeRootNode(NodeOps(ROOT_KEY));
  makeNode<StringNode>(NodeOps(*root, "name", ::Util::offsetOf(*this, mName)));
  makeNode<VectorNode<LuaGameObjectDescriptionNode>>(NodeOps(*root, "objects", ::Util::offsetOf(*this, mObjects)));
  return root;
}
