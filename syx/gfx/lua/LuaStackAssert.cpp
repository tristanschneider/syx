#include "Precompile.h"
#include "lua/LuaStackAssert.h"

#include "lua/LuaState.h"
#include <lua.hpp>

namespace Lua {
  StackAssert::StackAssert(lua_State* state, int expectedDiff)
    : mState(*state) {
    mExpectedStack = lua_gettop(&mState) + expectedDiff;
  }

  StackAssert::~StackAssert() {
    int top = lua_gettop(&mState);
    assert(mExpectedStack == top && "Stack size should be back to mStartStack size");
  }
}