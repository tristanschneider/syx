#include "Precompile.h"
#include "lua/LuaStackAssert.h"

#include "lua/LuaState.h"
#include <lua.hpp>

namespace Lua {
  StackAssert::StackAssert(State& state)
    : mState(state) {
    mStartStack = lua_gettop(mState);
  }

  StackAssert::~StackAssert() {
    assert(mStartStack == lua_gettop(mState) && "Stack size should be back to mStartStack size");
  }
}