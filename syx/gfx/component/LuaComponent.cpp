#include "Precompile.h"
#include "component/LuaComponent.h"

#include <lua.hpp>
#include "lua/LuaCompositeNodes.h"
#include "lua/LuaSandbox.h"
#include "lua/LuaStackAssert.h"
#include "lua/LuaState.h"
#include "lua/LuaUtil.h"

namespace {
  bool _isSupportedLuaPropType(int type) {
    switch(type) {
      case LUA_TLIGHTUSERDATA:
      case LUA_TNUMBER:
      case LUA_TSTRING:
      case LUA_TBOOLEAN:
      case LUA_TUSERDATA:
      case LUA_TTABLE:
        return true;
      default:
        return false;
    }
  }
}

DEFINE_EVENT(AddLuaComponentEvent, size_t owner, size_t script)
  , mOwner(owner)
  , mScript(script) {
}

DEFINE_EVENT(RemoveLuaComponentEvent, size_t owner, size_t script)
  , mOwner(owner)
  , mScript(script) {
}

DEFINE_COMPONENT(LuaComponent) {
  mScript = 0;
}

LuaComponent::LuaComponent(const LuaComponent& other)
  : Component(other.getType(), other.getOwner())
  , mScript(other.mScript) {
}

LuaComponent::~LuaComponent() {
  if(mProps) {
    auto props = static_cast<const Lua::BufferNode*>(mProps->getChild("props"));
    props->destroyBuffer(mPropsBuffer);
  }
}

std::unique_ptr<Component> LuaComponent::clone() const {
  return std::make_unique<LuaComponent>(*this);
}

void LuaComponent::set(const Component& component) {
  assert(getType() == component.getType() && "set component type must match");
  mScript = static_cast<const LuaComponent&>(component).mScript;
}

const ComponentTypeInfo& LuaComponent::getTypeInfo() const {
  static ComponentTypeInfo result("Script");
  return result;
}

const Lua::Node* LuaComponent::getLuaProps() const {
  return mProps.get();
}

std::unique_ptr<Lua::Node> LuaComponent::_buildLuaProps(lua_State* l) {
  using namespace Lua;
  auto root = makeRootNode(Lua::NodeOps(""));
  makeNode<LightUserdataSizetNode>(Lua::NodeOps(*root, "script", ::Util::offsetOf(*this, mScript)));
  Node& props = makeNode<BufferNode>(Lua::NodeOps(*root, "props", ::Util::offsetOf(*this, mPropsBuffer)));
  _buildPropsFromStack(l, props);
  return std::move(root);
}

void LuaComponent::_buildPropsFromStack(lua_State* l, Lua::Node& parent) const {
  Lua::StackAssert sa(l);
  //Dummy value for next to pop off
  lua_pushnil(l);
  bool first = true;
  size_t curSize = 0;
  Lua::Node* curParent = &parent;

  using namespace Lua;
  while(lua_next(l, -2)) {
    int keyType = lua_type(l, -2);
    bool isValidKey = keyType == LUA_TSTRING || lua_isinteger(l, -2);
    bool isValidValue = _isSupportedLuaPropType(lua_type(l, -1));
    if(!isValidKey || !isValidValue) {
      lua_pop(l, 1);
      continue;
    }

    //Key is now at -2 and value at -1
    const char* keyName = nullptr;
    NodeOps ops = keyType == LUA_TSTRING ?
      NodeOps(*curParent, lua_tostring(l, -2), curSize) :
      NodeOps(*curParent, static_cast<int>(lua_tointeger(l, -2)), curSize);

    Lua::Node* node = nullptr;

    switch(lua_type(l, -1)) {
      case LUA_TLIGHTUSERDATA: node = &makeNode<LightUserdataNode>(std::move(ops)); break;
      case LUA_TNUMBER: node = &makeNode<DoubleNode>(std::move(ops)); break;
      case LUA_TSTRING: node = &makeNode<StringNode>(std::move(ops)); break;
      case LUA_TBOOLEAN: node = &makeNode<BoolNode>(std::move(ops)); break;
      //TODO: get node from type name through metatable
      case LUA_TUSERDATA: break;
      case LUA_TTABLE:
        node = &makeNode<RootNode>(NodeOps(std::move(ops)));
        _buildPropsFromStack(l, *node);
        break;
    }

    if(node) {
      curSize += node->size();
    }

    lua_pop(l, 1);
  }
}

size_t LuaComponent::getScript() const {
  return mScript;
}

void LuaComponent::setScript(size_t script) {
  mScript = script;
}

void LuaComponent::init(Lua::State& state, int selfIndex) {
  Lua::StackAssert sa(state);
  assert(mScript && "Need a script to initilize");
  mSandbox = std::make_unique<Lua::Sandbox>(state, std::to_string(mOwner) + "_" + std::to_string(mScript));

  {
    auto sandbox = Lua::Sandbox::ScopedState(*mSandbox);
    //Run the script, filling the sandbox
    lua_pushvalue(state, -2);
    if(lua_pcall(state, 0, 0, 0) != LUA_OK) {
      printf("Error initializing script %i: %s\n", static_cast<int>(mScript), lua_tostring(state, -1));
      //Pop off the error
      lua_pop(state, 1);
    }
    else {
      //TODO: move ownership of props from per component to per script type
      mProps = _buildLuaProps(state);
      //Script load succeeded, call init if found
      int initFunc = lua_getfield(state, -1, "initialize");
      if(initFunc == LUA_TFUNCTION) {
        lua_pushvalue(state, selfIndex);
        _callFunc(state, "initialize", 1, 0);
      }
      else
        lua_pop(state, 1);
    }
  }
}

void LuaComponent::update(Lua::State& state, float dt, int selfIndex) {
  Lua::StackAssert sa(state);
  auto sandbox = Lua::Sandbox::ScopedState(*mSandbox);

  int updateType = lua_getfield(state, -1, "update");
  if(updateType == LUA_TFUNCTION) {
    lua_pushvalue(state, selfIndex);
    lua_pushnumber(state, dt);
    _callFunc(state, "update", 2, 0);
  }
  else
    lua_pop(state, 1);
}

bool LuaComponent::_callFunc(lua_State* s, const char* funcName, int arguments, int returns) const {
  if(int error = lua_pcall(s, arguments, returns, 0)) {
    //Error message is on top of the stack. Display then pop it
    printf("Error calling %s on object %i script %i: %s\n", funcName, static_cast<int>(mOwner), static_cast<int>(mScript), lua_tostring(s, -1));
    lua_pop(s, 1);
    return false;
  }
  return true;
}

void LuaComponent::uninit() {
  mSandbox = nullptr;
}

bool LuaComponent::needsInit() const {
  return mSandbox == nullptr;
}