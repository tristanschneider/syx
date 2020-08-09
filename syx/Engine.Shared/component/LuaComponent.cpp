#include "Precompile.h"
#include "component/LuaComponent.h"

#include "editor/InspectorFactory.h"
#include <lua.hpp>
#include "lua/LuaCompositeNodes.h"
#include "lua/LuaSandbox.h"
#include "lua/LuaStackAssert.h"
#include "lua/LuaState.h"
#include "lua/LuaUtil.h"
#include "lua/LuaVariant.h"
#include "Util.h"

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
  mProps = std::make_unique<Lua::Variant>();
}

LuaComponent::LuaComponent(const LuaComponent& other)
  : Component(other.getType(), other.getOwner())
  , mScript(other.mScript)
  , mProps(std::make_unique<Lua::Variant>(other.mProps ? *other.mProps : Lua::Variant())) {
  setSubType(mScript);
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

void LuaComponent::_setSubType(size_t subType) {
  mScript = subType;
}

void LuaComponent::onPropsUpdated() {
  mPropsNeedWriteToLua = true;
  setSubType(mScript);
}

const Lua::Node* LuaComponent::getLuaProps() const {
  static auto props = _buildLuaProps();
  return props.get();
}

std::unique_ptr<Lua::Node> LuaComponent::_buildLuaProps() const {
  using namespace Lua;
  using namespace Inspector;
  auto root = makeRootNode(Lua::NodeOps(""));
  makeNode<LightUserdataSizetNode>(Lua::NodeOps(*root, "script", ::Util::offsetOf(*this, mScript))).setInspector(getAssetInspector(*getAssetRepo(), "lc"));
  Node& propsPtr = makeNode<UniquePtrNode<Variant>>(Lua::NodeOps(*root, "", ::Util::offsetOf(*this, mProps)));
  makeNode<VariantNode>(Lua::NodeOps(propsPtr, "props", 0));
  return std::move(root);
}

size_t LuaComponent::getScript() const {
  return mScript;
}

void LuaComponent::setScript(size_t script) {
  mScript = script;
}

void LuaComponent::init(Lua::State& state, int) {
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
      //Write state in case anything was loaded with this component
      //TODO: is this actually handling saved values properly?
      _writePropsToLua(state);
      //Read state in case there wasn't to populate defaults from script
      _readPropsFromLua(state);
      mNeedsInit = true;
    }
  }
}

void LuaComponent::update(Lua::State& state, float dt, int selfIndex) {
  Lua::StackAssert sa(state);
  auto sandbox = Lua::Sandbox::ScopedState(*mSandbox);

  if(mPropsNeedWriteToLua) {
    _writePropsToLua(state);
    mPropsNeedWriteToLua = false;
  }

  if(mNeedsInit) {
    Lua::StackAssert ia(state);
    //Script load succeeded, call init if found
    int initFunc = lua_getfield(state, -1, "initialize");
    if(initFunc == LUA_TFUNCTION) {
      lua_pushvalue(state, selfIndex);
      _callFunc(state, "initialize", 1, 0);
      _readPropsFromLua(state);
    }
    else
      lua_pop(state, 1);
    mNeedsInit = false;
  }

  int updateType = lua_getfield(state, -1, "update");
  if(updateType == LUA_TFUNCTION) {
    lua_pushvalue(state, selfIndex);
    lua_pushnumber(state, dt);
    _callFunc(state, "update", 2, 0);
    //TODO: only do this when props have changed
    _readPropsFromLua(state);
  }
  else
    lua_pop(state, 1);
}

void LuaComponent::_readPropsFromLua(lua_State* s) {
  //Read from sandbox table on top of stack
  mProps->readFromLua(s);
}

void LuaComponent::_writePropsToLua(lua_State* s) {
  //Avoid pushin the root node so the existing sandbox table is merged with the props table instead of replaced
  mProps->forEachChild([this, s](const Lua::Variant& child) {
    child.getKey().push(s);
    child.writeToLua(s);
    lua_settable(s, -3);
  });
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

const Lua::Variant& LuaComponent::getPropVariant() const {
  return *mProps;
}
