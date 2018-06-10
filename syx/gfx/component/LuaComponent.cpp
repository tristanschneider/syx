#include "Precompile.h"
#include "component/LuaComponent.h"

#include <lua.hpp>
#include "lua/LuaNode.h"
#include "lua/LuaSandbox.h"
#include "lua/LuaStackAssert.h"
#include "lua/LuaState.h"
#include "lua/LuaUtil.h"

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
  //TODO: handle properties inside scripts
  static std::unique_ptr<Lua::Node> props = _buildLuaProps();
  return props.get();
}

std::unique_ptr<Lua::Node> LuaComponent::_buildLuaProps() const {
  using namespace Lua;
  auto root = makeRootNode(Lua::NodeOps(""));
  makeNode<LightUserdataSizetNode>(Lua::NodeOps(*root, "script", ::Util::offsetOf(*this, mScript)));
  return std::move(root);
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
    if(int error = lua_pcall(state, 0, 0, 0)) {
      printf("Error initializing script %i: %s\n", static_cast<int>(mScript), lua_tostring(state, -1));
      //Pop off the error
      lua_pop(state, 1);
    }
    else {
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