#include "Precompile.h"
#include "lua/LuaState.h"

#include <lua.hpp>

namespace Lua {
  State::State() {
    mState = luaL_newstate();
    luaL_openlibs(mState);
  }

  State::State(State&& s) {
    mState = s.mState;
    s.mState = nullptr;
  }

  State::~State() {
    if(mState)
      lua_close(mState);
  }

  State::operator lua_State*() {
    return mState;
  }

  lua_State* State::get() {
    return mState;
  }
}