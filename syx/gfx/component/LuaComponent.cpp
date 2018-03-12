#include "Precompile.h"
#include "component/LuaComponent.h"

#include <lua.hpp>
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

LuaComponent::~LuaComponent() {
}

size_t LuaComponent::getScript() const {
  return mScript;
}

void LuaComponent::setScript(size_t script) {
  mScript = script;
}

void LuaComponent::init(Lua::State& state) {
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
  }
}

void LuaComponent::update(Lua::State& state, float dt) {
  Lua::StackAssert sa(state);
  auto sandbox = Lua::Sandbox::ScopedState(*mSandbox);

  int updateType = lua_getfield(state, -1, "update");
  if(updateType == LUA_TFUNCTION) {
    lua_pushnumber(state, dt);
    if(int error = lua_pcall(state, 1, 0, 0)) {
      //Error message is on top of the stack. Display then pop it
      printf("Error updating object %i script %i: %s\n", static_cast<int>(mOwner), static_cast<int>(mScript), lua_tostring(state, -1));
      lua_pop(state, 1);
    }
  }
}

void LuaComponent::uninit() {
  mSandbox = nullptr;
}

bool LuaComponent::needsInit() const {
  return mSandbox == nullptr;
}