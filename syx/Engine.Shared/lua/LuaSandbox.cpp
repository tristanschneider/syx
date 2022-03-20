#include "Precompile.h"
#include "lua/LuaSandbox.h"

#include "lua/LuaStackAssert.h"
#include "lua/LuaState.h"
#include <lua.hpp>

#include "lua/LuaUtil.h"

namespace Lua {
  const char* Sandbox::CHUNK_ID = "_chunk_";

  Sandbox::Sandbox(IState& state, const std::string& id)
    : mState(&state)
    , mId(id) {
    _createSandbox();
    _setUpvalue();
  }

  Sandbox::~Sandbox() {
    _destroySandbox();
  }

  Sandbox::Sandbox(Sandbox&& rhs)
    : mState(std::move(rhs.mState))
    , mId(std::move(rhs.mId)) {
    rhs._clear();
  }

  Sandbox& Sandbox::operator=(Sandbox&& rhs) {
    _destroySandbox();
    mState = rhs.mState;
    mId = std::move(rhs.mId);
    rhs._clear();
    return *this;
  }

  void Sandbox::push() {
    lua_getglobal(*mState, mId.c_str());

    {
      StackAssert sa(*mState);
      lua_getfield(*mState, -1, CHUNK_ID);
      lua_pushvalue(*mState, -2);
      const char* set = lua_setupvalue(*mState, -2, 1);
      set;
      assert(strcmp(set, "_ENV") == 0 && "Should have set environment");
      lua_pop(*mState, 1);
    }
  }

  void Sandbox::pop() {
    lua_pop(*mState, 1);
  }

  void Sandbox::_createSandbox() {
    IState& l = *mState;
    StackAssert sa(l);

    //Push clean environment
    lua_newtable(l);

    //Make a local environment to sandbox new variables, but expose the existing globals through __index to _G
    //set metatable to { __index = _G }
    lua_newtable(l);
    int globalType = lua_getglobal(l, "_G");
    globalType;
    assert(globalType != LUA_TNIL && "No global table to forward sandbox index calls to");
    lua_setfield(l, -2, "__index");
    lua_setmetatable(l, -2);

    //Copy the chunk into our table so we can set the upvalues on it later
    assert(lua_type(l, -2) == LUA_TFUNCTION && "The chunk this is sandboxing should be on top of the stack");
    lua_pushvalue(l, -2);
    lua_setfield(l, -2, CHUNK_ID);

    //environment table is now on top of the stack, put that in the global table
    lua_setglobal(l, mId.c_str());
  }

  void Sandbox::_destroySandbox() {
    StackAssert sa(*mState);
    //Set our global table entry to null
    lua_pushnil(*mState);
    lua_setglobal(*mState, mId.c_str());
  }

  void Sandbox::_setUpvalue() {
    StackAssert sa(*mState);
    lua_getglobal(*mState, mId.c_str());
    //Set _ENV on the top function to our sandbox table
    const char* up = lua_setupvalue(*mState, -2, 1);
    up;
    assert(strcmp(up, "_ENV") == 0 && "Should have been a function with _ENV upvalue on top of stack"); 
  }

  void Sandbox::_clear() {
    mState = nullptr;
    mId.clear();
  }
}