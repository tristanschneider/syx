#include "Precompile.h"
#include "LuaGameObject.h"

#include "component/LuaComponent.h"
#include "component/SpaceComponent.h"
#include "event/BaseComponentEvents.h"
#include "event/EventBuffer.h"
#include "lua/LuaCache.h"
#include "lua/LuaComponentNode.h"
#include "lua/LuaCompositeNodes.h"
#include "lua/LuaGameContext.h"
#include "lua/LuaStackAssert.h"
#include "lua/LuaUtil.h"
#include <lua.hpp>
#include "system/LuaGameSystem.h"


const std::string LuaGameObject::CLASS_NAME = "Gameobject";
// TODO: instead of static, this should be owned by the context and accessible as a global lua object like the ILuaGameContext
std::unique_ptr<Lua::Cache> LuaGameObject::sCache = std::make_unique<Lua::Cache>("_goc_", CLASS_NAME);

namespace {
  using LuaComp = std::unique_ptr<LuaComponent>;
  using LuaComps = std::vector<LuaComp>;
  LuaComps::iterator _findLuaComp(size_t script, LuaComps& comps) {
    return std::find_if(comps.begin(), comps.end(), [script](const LuaComp& c) {
      return c->getScript() == script;
    });
  }
  LuaComps::const_iterator _findLuaComp(size_t script, const LuaComps& comps) {
    return std::find_if(comps.begin(), comps.end(), [script](const LuaComp& c) {
      return c->getScript() == script;
    });
  }
}

LuaGameObject::LuaGameObject(Handle h)
  : mHandle(h)
  , mTransform(h)
  , mSpace(h)
  , mName(h) {
  _addBuiltInComponents();
}

LuaGameObject::~LuaGameObject() {
}

void LuaGameObject::_addBuiltInComponents() {
  _forEachBuiltInComponent([this](Component& c) {
    _addComponentLookup(c);
  });
}

bool LuaGameObject::_isBuiltInComponent(const Component& comp) const {
  size_t compType = comp.getType();
  bool isBuiltIn = false;
  _forEachBuiltInComponent([compType, &isBuiltIn](const Component& c) {
    isBuiltIn = isBuiltIn || c.getType() == compType;
  });
  return isBuiltIn;
}

Handle LuaGameObject::getHandle() const {
  return mHandle;
}

void LuaGameObject::addComponent(std::unique_ptr<Component> component) {
  bool wasBuiltIn = false;
  //For built in types copy value to type so it behaves as other components by being overwritten
  _forEachBuiltInComponent([&component, &wasBuiltIn](Component& c) {
    if(c.getType() == component->getType()) {
      c.set(*component);
      wasBuiltIn = true;
    }
  });
  if(wasBuiltIn)
    return;

  if(component->getType() == Component::typeId<LuaComponent>()) {
    addLuaComponent(std::unique_ptr<LuaComponent>(static_cast<LuaComponent*>(component.release())));
  }
  else {
    _addComponentLookup(*component);
    mComponents[component->getType()] = std::move(component);
  }
}

void LuaGameObject::removeComponent(size_t type) {
  //TODO: get rid of this overload
  removeComponent({ type, 0 });
}

void LuaGameObject::removeComponent(const ComponentType& type) {
  if(type.id == Component::typeId<LuaComponent>()) {
    auto found = _findLuaComp(type.subId, mLuaComponents);
    if(found != mLuaComponents.end()) {
      _removeComponentLookup(**found);
      mLuaComponents.erase(found);
    }
  }
  else if(std::unique_ptr<Component>* component = mComponents.get(type.id)) {
    if(*component) {
      _removeComponentLookup(**component);
      *component = nullptr;
    }
  }
  //Built in components can't be removed
}

Component* LuaGameObject::getComponent(size_t type, size_t subType) {
  return getComponent({ type, subType });
}

const Component* LuaGameObject::getComponent(size_t type, size_t subType) const {
  return getComponent({ type, subType });
}

Component* LuaGameObject::getComponent(const ComponentType& type) {
  return const_cast<Component*>(const_cast<const LuaGameObject*>(this)->getComponent(type));
}

const Component* LuaGameObject::getComponent(const ComponentType& type) const {
  const Component* builtIn = nullptr;
  _forEachBuiltInComponent([&builtIn, type](const Component& c) {
    if(type.id == c.getType()) {
      builtIn = &c;
    }
  });
  if(builtIn) {
    return builtIn;
  }

  if(type.id == Component::typeId<LuaComponent>()) {
    return getLuaComponent(type.subId);
  }
  auto ptr = mComponents.get(type.id);
  return ptr ? ptr->get() : nullptr;
}

const Component* LuaGameObject::getComponent(const char* name) const {
  const auto& it = mHashToComponent.find(Util::constHash(name));
  return it != mHashToComponent.end() ? it->second : nullptr;
}

Transform& LuaGameObject::getTransform() {
  return mTransform;
}

const Transform& LuaGameObject::getTransform() const {
  return mTransform;
}

const NameComponent& LuaGameObject::getName() const {
  return mName;
}

Handle LuaGameObject::getSpace() const {
  return mSpace.get();
}

void LuaGameObject::remove(EventBuffer& msg) const {
  forEachComponent([&msg, this](const Component& comp) {
    msg.push(RemoveComponentEvent(getHandle(), comp.getType(), comp.getSubType()));
  });
  msg.push(RemoveGameObjectEvent(getHandle()));
}

LuaComponent* LuaGameObject::addLuaComponent(std::unique_ptr<LuaComponent> component) {
  const size_t script = component->getScript();
  LuaComponent& result = *component;
  mLuaComponents.emplace_back(std::move(component));
  _addComponentLookup(result);
  return &result;
}

LuaComponent* LuaGameObject::addLuaComponent(size_t script) {
  mLuaComponents.emplace_back(std::make_unique<LuaComponent>(mHandle));
  LuaComponent& result = *mLuaComponents.back();
  result.setScript(script);
  _addComponentLookup(result);
  return &result;
}

LuaComponent* LuaGameObject::getLuaComponent(size_t script) {
  return const_cast<LuaComponent*>(const_cast<const LuaGameObject*>(this)->getLuaComponent(script));
}

const LuaComponent* LuaGameObject::getLuaComponent(size_t script) const {
  auto it = _findLuaComp(script, mLuaComponents);
  return it != mLuaComponents.end() ? it->get() : nullptr;
}

void LuaGameObject::removeLuaComponent(size_t script) {
  auto it = _findLuaComp(script, mLuaComponents);
  if(it != mLuaComponents.end()) {
    _removeComponentLookup(**it);
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

const TypeMap<std::unique_ptr<Component>, Component>& LuaGameObject::getComponents() const {
  return mComponents;
}

size_t LuaGameObject::componentCount() const {
  return mHashToComponent.size();
}

std::unique_ptr<LuaGameObject> LuaGameObject::clone() const {
  auto result = std::make_unique<LuaGameObject>(mHandle);
  forEachComponent([&result](const Component& c) {
    result->addComponent(c.clone());
  });
  return result;
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
  IGameObject& self = getObj(l, 1);
  std::string result = CLASS_NAME + "( " + std::to_string(self.getHandle()) + ")";
  lua_pushlstring(l, result.c_str(), result.size());
  return 1;
}

int LuaGameObject::indexOverload(lua_State* l) {
  IGameObject& obj = getObj(l, 1);
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
  if(IComponent* comp = obj.getComponentByPropName(key)) {
    Component::push(l, *comp);
  }
  else {
    //No component by this name, return null
    lua_pushnil(l);
  }
  return 1;
}

int LuaGameObject::newIndexOverload(lua_State* l) {
  IGameObject& self = getObj(l, 1);
  const char* key = luaL_checkstring(l, 2);
  int valueType = lua_type(l, 3);
  luaL_argcheck(l, valueType == LUA_TTABLE || valueType == LUA_TNIL, 3, "nil or table expected");
  ILuaGameContext& game = Lua::checkGameContext(l);

  //Trying to add component
  if(valueType == LUA_TTABLE) {
    IComponent* comp = self.addComponentFromPropName(key);
    luaL_argcheck(l, comp != nullptr, 2, "no such component exists");
    lua_pushvalue(l, 3);
    Component::setPropsFromStack(l, *comp);
    lua_pop(l, 1);
  }
  //Trying to remove component. Setting invalid component name to nil is fine, as it wouldn't do anything
  else {
    game.removeComponentFromPropName(key, self.getHandle());
  }
  return 0;
}

int LuaGameObject::addComponent(lua_State* l) {
  IGameObject& obj = getObj(l, 1);
  const char* componentName = luaL_checkstring(l, 2);
  if(IComponent* result = obj.addComponent(componentName)) {
    Component::push(l, *result);
  }
  else {
    lua_pushnil(l);
  }
  return 1;
}

int LuaGameObject::removeComponent(lua_State* l) {
  IGameObject& obj = getObj(l, 1);
  const char* componentName = luaL_checkstring(l, 2);
  ILuaGameContext& game = Lua::checkGameContext(l);
  game.removeComponent(componentName, obj.getHandle());
  return 0;
}

int LuaGameObject::isValid(lua_State* l) {
  lua_pushboolean(l, sCache->getParam(l, 1) != nullptr);
  return 1;
}

int LuaGameObject::newDefault(lua_State* l) {
  Lua::StackAssert sa(l, 1);
  ILuaGameContext& game = Lua::checkGameContext(l);
  LuaGameObject::push(l, game.addGameObject());
  return 1;
}

int LuaGameObject::push(lua_State* l, IGameObject& obj) {
  return sCache->push(l, &obj, obj.getHandle());
}

int LuaGameObject::invalidate(lua_State* l, const IGameObject& obj) {
  sCache->invalidate(l, obj.getHandle());
  obj.forEachComponent([l](const Component& comp) {
    comp.invalidate(l);
  });
  return 0;
}

IGameObject& LuaGameObject::getObj(lua_State* l, int index) {
  return *static_cast<IGameObject*>(sCache->checkParam(l, index));
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
  static std::unique_ptr<Lua::Node> result = [this]() {
    using namespace Lua;
    auto root = makeRootNode(NodeOps(""));
    makeNode<SizetNode>(NodeOps(*root, "handle", ::Util::offsetOf(*this, mHandle)));
    makeNode<VectorNode<ComponentNode>>(NodeOps(*root, "components", ::Util::offsetOf(*this, mComponents)));
    return root;
  }();
  return *result;
}

const char* LuaSceneDescription::ROOT_KEY = "scene";
const char* LuaSceneDescription::FILE_EXTENSION = "ls";

const Lua::Node& LuaSceneDescription::getMetadata() const {
  static std::unique_ptr<Lua::Node> result = [this]() {
    using namespace Lua;
    auto root = makeRootNode(NodeOps(ROOT_KEY));
    makeNode<StringNode>(NodeOps(*root, "name", ::Util::offsetOf(*this, mName)));
    makeNode<VectorNode<LuaGameObjectDescriptionNode>>(NodeOps(*root, "objects", ::Util::offsetOf(*this, mObjects)));
    makeNode<VectorNode<StringNode>>(NodeOps(*root, "assets", ::Util::offsetOf(*this, mAssets)));
    return root;
  }();
  return *result;
}